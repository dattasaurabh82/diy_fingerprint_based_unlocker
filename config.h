// ============================================================
// config.h — Pin definitions, timing constants, global config
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

// ─── Firmware ───
#define FW_VERSION "1.0.0"

// ─── Pin Map ───
#define PIN_SENSOR_TX    0   // UART0 TX → Sensor RX (Black wire)
#define PIN_SENSOR_RX    1   // UART0 RX ← Sensor TX (Yellow wire)
#define PIN_IRQ          2   // Sensor IRQ (reserved, unused v1)
#define PIN_MODE_SWITCH  3   // SPDT switch (other leg to GND)

// ─── Switch ───
#define DEBOUNCE_MS      50

// ─── Sensor ───
#define SENSOR_BAUD      115200
#define SENSOR_INIT_DELAY_MS  200   // let sensor wake after UART start

// ─── Fingerprint ───
#define COLLECT_COUNT    3    // captures per enrollment
#define CAPTURE_TIMEOUT  10   // seconds per capture attempt
#define MATCH_TIMEOUT    5    // seconds for recognition capture
#define MAX_CAPTURE_RETRIES 3 // retries per capture step

// ─── Password ───
#define PASSWORD_MAX_LEN  32
#define PASSWORD_TIMEOUT_MS 30000  // 30s to enter password
#define PASSWORD_MAX_CONFIRM_ATTEMPTS 3

// ─── EEPROM Layout ───
#define EEPROM_SIZE       64   // bytes to init (only use 36)
#define EEPROM_ADDR_MAGIC       0x00
#define EEPROM_ADDR_ACTIVE_SLOT 0x01
#define EEPROM_ADDR_PWD_LEN     0x02
#define EEPROM_ADDR_PWD_START   0x03
#define EEPROM_ADDR_CHECKSUM    0x23  // 0x03 + 32
#define EEPROM_MAGIC_VALUE      0xA5

// ─── HID Timing ───
#define LOCK_DELAY_MS        2000
#define WAKE_PRESSES         2
#define WAKE_PRESS_DELAY_MS  200
#define WAKE_SETTLE_MS       2000
#define FIELD_CLEAR_DELAY_MS 200
#define POST_TYPE_DELAY_MS   100
#define POST_ENTER_DELAY_MS  500

// ─── Cooldown ───
#define COOLDOWN_MS          5000

// ─── Debug ───
// Uncomment to enable verbose debug output
// #define DEBUG_VERBOSE

#endif // CONFIG_H
