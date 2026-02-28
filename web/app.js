// ══════════════════════════════════════════════
// DIY RP2350 Fingerprint Unlock System — Monitor
// Web Serial + xterm.js application logic
// ══════════════════════════════════════════════

// ── Config ──
const BAUD_RATE = 115200;
const USB_VID_RASPBERRY_PI = 0x2E8A;
const RECONNECT_INTERVAL_MS = 1500;
const RECONNECT_MAX_ATTEMPTS = 10;
const RECONNECT_STABLE_MS = 6000; // stay in reconnect-ready mode after connect (RP2350 USB re-enumerates twice: CDC then CDC+HID)

// ── State ──
let port = null;
let reader = null;
let writer = null;
let readLoopActive = false;
let isReconnecting = false;
let reconnectAttempts = 0;
let stabilityTimer = null;
let passwordMode = false;   // true when firmware is prompting for password
let incomingLineBuf = '';    // accumulates incoming chars to detect prompts

// ── DOM refs ──
const btnConnect = document.getElementById('btn-connect');
const btnClear = document.getElementById('btn-clear');
const btnReset = document.getElementById('btn-reset');
const btnSend = document.getElementById('btn-send');
const serialInput = document.getElementById('serial-input');
const inputBar = document.getElementById('input-bar');
const statusDot = document.getElementById('status-dot');
const statusText = document.getElementById('status-text');

// ── Mobile / browser detection ──
const isMobile = /Android|iPhone|iPad|iPod|webOS|Opera Mini/i.test(navigator.userAgent);
const hasWebSerial = 'serial' in navigator;

if (isMobile) {
  document.getElementById('mobile-warning').style.display = 'flex';
  document.getElementById('desktop-content').style.display = 'none';
} else if (!hasWebSerial) {
  document.getElementById('browser-warning').style.display = 'flex';
  document.getElementById('desktop-content').style.display = 'none';
}

// ── Terminal font metrics (must match Terminal options below) ──
const FONT_SIZE = 14;
const LINE_HEIGHT = 1.2;
const CELL_HEIGHT = Math.ceil(FONT_SIZE * LINE_HEIGHT);
const CELL_WIDTH = 8.4;  // approximate for 14px monospace
const TERM_PAD_LEFT = 24;  // matches #terminal-container padding-left
const TERM_PAD_OTHER = 8;  // matches other padding

// Calculate terminal size from actual container dimensions (set by flexbox)
function calcTermSize() {
  const container = document.getElementById('terminal-container');
  const availW = container.clientWidth - TERM_PAD_LEFT - TERM_PAD_OTHER;
  const availH = container.clientHeight - TERM_PAD_OTHER * 2;
  return {
    cols: Math.max(40, Math.floor(availW / CELL_WIDTH)),
    rows: Math.max(10, Math.floor(availH / CELL_HEIGHT))
  };
}

const initSize = calcTermSize();

// ── xterm.js setup (read-only terminal) ──
const term = new window.Terminal({
  cols: initSize.cols,
  rows: initSize.rows,
  fontSize: FONT_SIZE,
  lineHeight: LINE_HEIGHT,
  fontFamily: "'JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Menlo', monospace",
  scrollback: 5000,
  convertEol: true,
  cursorBlink: false,
  disableStdin: true,
  theme: {
    // Nord dark
    background: '#2E3440',
    foreground: '#D8DEE9',
    cursor: '#88C0D0',
    cursorAccent: '#2E3440',
    black: '#3B4252',
    red: '#BF616A',
    green: '#A3BE8C',
    yellow: '#EBCB8B',
    blue: '#81A1C1',
    magenta: '#B48EAD',
    cyan: '#88C0D0',
    white: '#E5E9F0',
    brightBlack: '#4C566A',
    brightRed: '#BF616A',
    brightGreen: '#A3BE8C',
    brightYellow: '#EBCB8B',
    brightBlue: '#81A1C1',
    brightMagenta: '#B48EAD',
    brightCyan: '#8FBCBB',
    brightWhite: '#ECEFF4',
  }
});

const termContainer = document.getElementById('terminal-container');
term.open(termContainer);

// After open, re-measure (container now has its flex height) and resize
requestAnimationFrame(() => {
  const size = calcTermSize();
  term.resize(size.cols, size.rows);
});

// Resize terminal on window resize
window.addEventListener('resize', () => {
  const size = calcTermSize();
  term.resize(size.cols, size.rows);
});

// Welcome message
term.writeln('\x1b[36m── DIY RP2350 Fingerprint Unlock System [Monitor] ──\x1b[0m');
term.writeln('\x1b[90mClick "Connect" to open a serial connection.\x1b[0m');
term.writeln('');

// ── UI state helpers ──
function setConnected(connected) {
  statusDot.classList.remove('connected', 'reconnecting');
  if (connected) statusDot.classList.add('connected');
  statusText.textContent = connected ? 'Connected' : 'Disconnected';
  btnConnect.textContent = connected ? 'Disconnect' : 'Connect';
  btnConnect.disabled = false;
  btnReset.disabled = !connected;
  btnSend.disabled = !connected;
  serialInput.disabled = !connected;
  serialInput.placeholder = connected ? 'Type a command and press Enter...' : 'Not connected';
  if (connected) serialInput.focus();
}

function setReconnecting() {
  statusDot.classList.remove('connected');
  statusDot.classList.add('reconnecting');
  statusText.textContent = 'Reconnecting...';
  btnConnect.textContent = 'Cancel';
  btnConnect.disabled = false;
  btnReset.disabled = true;
  btnSend.disabled = true;
  serialInput.disabled = true;
  serialInput.placeholder = 'Reconnecting to device...';
}

// ── Send text over serial ──
async function serialSend(text) {
  if (writer) {
    const encoder = new TextEncoder();
    await writer.write(encoder.encode(text));
  }
}

// ── Connect (initial — requires user gesture for port picker) ──
async function connect() {
  try {
    port = await navigator.serial.requestPort({
      filters: [{ usbVendorId: USB_VID_RASPBERRY_PI }]
    });
    await openPort();
  } catch (err) {
    if (err.name !== 'NotFoundError') {
      term.writeln(`\x1b[31mConnection error: ${err.message}\x1b[0m`);
    }
  }
}

// ── Open an already-granted port ──
async function openPort() {
  try {
    await port.open({ baudRate: BAUD_RATE });

    const wasReconnecting = isReconnecting;
    setConnected(true);
    reconnectAttempts = 0;
    term.writeln('\x1b[32m── Connected ──\x1b[0m');
    term.writeln('');

    // Set up writer
    writer = port.writable.getWriter();

    // Start read loop
    readLoopActive = true;
    readLoop();

    // Listen for disconnect
    port.addEventListener('disconnect', onPortDisconnect);

    // After a reset-reconnect, the RP2350 may re-enumerate USB a second time
    // (CDC init, then CDC+HID composite). Stay in reconnect-ready mode for a
    // grace period so the second disconnect also auto-reconnects.
    if (wasReconnecting) {
      isReconnecting = true;  // keep the flag alive
      if (stabilityTimer) clearTimeout(stabilityTimer);
      stabilityTimer = setTimeout(() => {
        isReconnecting = false;
        stabilityTimer = null;
      }, RECONNECT_STABLE_MS);
    } else {
      isReconnecting = false;
    }

  } catch (err) {
    // Port not ready yet (e.g. during reconnect)
    throw err;
  }
}

// ── Read loop ──
async function readLoop() {
  const decoder = new TextDecoder();
  try {
    while (port && port.readable && readLoopActive) {
      reader = port.readable.getReader();
      try {
        while (true) {
          const { value, done } = await reader.read();
          if (done) break;
          if (value) {
            const text = decoder.decode(value);
            term.write(text);
            checkForPasswordPrompt(text);
          }
        }
      } finally {
        reader.releaseLock();
        reader = null;
      }
    }
  } catch (err) {
    if (readLoopActive) {
      // Suppress error during reconnect — it's expected
      if (!isReconnecting) {
        term.writeln(`\x1b[31mRead error: ${err.message}\x1b[0m`);
      }
    }
  }
}

// ── Disconnect (user-initiated) ──
async function disconnect() {
  isReconnecting = false;
  reconnectAttempts = 0;
  setPasswordMode(false);
  incomingLineBuf = '';
  if (stabilityTimer) { clearTimeout(stabilityTimer); stabilityTimer = null; }
  await closePort();
  port = null;
  setConnected(false);
  term.writeln('');
  term.writeln('\x1b[33m── Disconnected ──\x1b[0m');
}

// ── Close port cleanly ──
async function closePort() {
  readLoopActive = false;

  if (reader) {
    try { await reader.cancel(); } catch { }
    reader = null;
  }

  if (writer) {
    try { writer.releaseLock(); } catch { }
    writer = null;
  }

  if (port) {
    try { await port.close(); } catch { }
  }
}

// ── Handle unexpected disconnect (USB unplug or reset reboot) ──
function onPortDisconnect() {
  readLoopActive = false;
  reader = null;
  writer = null;

  // If this was triggered by a reset, auto-reconnect
  if (isReconnecting) {
    // Port object is now stale — we need to get the new one
    port = null;
    term.writeln('\x1b[90m── Device rebooting, waiting for reconnect... ──\x1b[0m');
    setReconnecting();
    attemptReconnect();
  } else {
    port = null;
    setConnected(false);
    term.writeln('');
    term.writeln('\x1b[33m── Device disconnected ──\x1b[0m');
  }
}

// ── Auto-reconnect (after reset) ──
async function attemptReconnect() {
  while (reconnectAttempts < RECONNECT_MAX_ATTEMPTS && isReconnecting) {
    reconnectAttempts++;
    await sleep(RECONNECT_INTERVAL_MS);

    if (!isReconnecting) return; // cancelled

    try {
      // Get previously-granted ports (no picker needed)
      const ports = await navigator.serial.getPorts();
      const matchingPort = ports.find(p => {
        const info = p.getInfo();
        return info.usbVendorId === USB_VID_RASPBERRY_PI;
      });

      if (matchingPort) {
        port = matchingPort;
        await openPort();
        return; // success
      }
    } catch (err) {
      // Port not ready yet — keep trying
    }
  }

  // Give up
  if (isReconnecting) {
    isReconnecting = false;
    setConnected(false);
    term.writeln('\x1b[31m── Reconnect failed — click Connect to try again ──\x1b[0m');
  }
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

// ── Button handlers ──
btnConnect.addEventListener('click', () => {
  if (isReconnecting) {
    // Cancel reconnect
    isReconnecting = false;
    reconnectAttempts = 0;
    if (stabilityTimer) { clearTimeout(stabilityTimer); stabilityTimer = null; }
    port = null;
    setConnected(false);
    term.writeln('\x1b[33m── Reconnect cancelled ──\x1b[0m');
  } else if (port) {
    disconnect();
  } else {
    connect();
  }
});

btnClear.addEventListener('click', () => {
  term.clear();
});

btnReset.addEventListener('click', async () => {
  if (writer) {
    // Flag that we expect a disconnect and should auto-reconnect
    isReconnecting = true;
    reconnectAttempts = 0;
    await serialSend('!RESET\n');
    term.writeln('');
    term.writeln('\x1b[33m── Reset command sent ──\x1b[0m');
  }
});

// ── Input bar: send on Enter or Send button ──
serialInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter' && !serialInput.disabled) {
    sendInputValue();
  }
});

btnSend.addEventListener('click', () => {
  sendInputValue();
});

function sendInputValue() {
  const val = serialInput.value;
  if (val.length === 0) return;
  serialSend(val + '\n');
  serialInput.value = '';
  serialInput.focus();
}

// ── Password prompt detection ──
// Watches incoming serial text for password prompt lines.
// Toggles input field between password (masked) and text (plain).
function checkForPasswordPrompt(text) {
  // Accumulate into line buffer and check complete lines
  for (const ch of text) {
    if (ch === '\n' || ch === '\r') {
      const line = incomingLineBuf.trim();
      if (line.length > 0) {
        if (line.includes('Enter password') || line.includes('Confirm password')) {
          setPasswordMode(true);
        } else if (line.includes('Mismatch') || line.includes('Empty password')) {
          // Password error — flash but stay in password mode
          flashInputError();
        } else if (line.includes('Password entry timeout') || line.includes('Too many mismatches')) {
          // Fatal password error — exit password mode with flash
          flashInputError();
          setPasswordMode(false);
        } else if (passwordMode && (
          line.includes('[REG]') || line.includes('[AUTH]') ||
          line.includes('[BOOT]') || line.includes('[MODE]') ||
          line.includes('[CMD]') || line.includes('[ERROR]')
        )) {
          // Any non-password tagged line ends password mode
          setPasswordMode(false);
        }
      }
      incomingLineBuf = '';
    } else {
      incomingLineBuf += ch;
    }
  }
}

function setPasswordMode(on) {
  passwordMode = on;
  serialInput.type = on ? 'password' : 'text';
  serialInput.placeholder = on ? 'Type password (hidden)...' : 'Type a command and press Enter...';
  btnSend.textContent = on ? 'Submit' : 'Send';
  inputBar.classList.toggle('password-mode', on);
  if (on && !serialInput.disabled) {
    serialInput.focus();
  }
}

// Flash the input bar on mismatch / error messages
function flashInputError() {
  inputBar.classList.remove('error-flash');
  // Force reflow to restart animation
  void inputBar.offsetWidth;
  inputBar.classList.add('error-flash');
}
