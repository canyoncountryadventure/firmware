#ifndef _SEEED_XIAO_NRF52840_KIT_H_
#define _SEEED_XIAO_NRF52840_KIT_H_

/** Master clock frequency */
#define VARIANT_MCK (64000000ul)

#define USE_LFXO // Board uses 32khz crystal for LF
// #define USE_LFRC    // Board uses RC for LF

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "WVariant.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/*
Xiao pin assignments

| Pin   | Default  | I2C  | BTB  | BLE-L |     | Pin   | Default | I2C  | BTB  | BLE-L |
| ----- | -------- | ---- | ---- | ----- | --- | ----- | ------- | ---- | ---- | ----- |
|       |          |      |      |       |     |       |         |      |      |       |
| D0    | G_STBY   | UBTN | DIO1 | CS    |     | 5v    |         |      |      |       |
| D1    | DIO1     | DIO1 | Busy | DIO1  |     | GND   |         |      |      |       |
| D2    | NRST     | NRST | NRST | Busy  |     | 3v3   |         |      |      |       |
| D3    | Busy     | Busy | CS   | NRST  |     | D10   | MOSI    | MOSI | MOSI | MOSI  |
| D4    | CS       | CS   | RXEN | SDA   |     | D9    | MISO    | MISO | MISO | MISO  |
| D5    | RXEN     | RXEN |      | SCL   |     | D8    | SCK     | SCK  | SCK  | SCK   |
| D6    | G_TX     | SDA  | G_TX |       |     | D7    | G_RX    | SCL  | G_RX | RXEN  |
|       |          |      |      |       |     |       |         |      |      |       |
|       | End      |      |      |       |     |       |         |      |      |       |
| NFC1/ | SDA      | G_TX | SDA  | G_TX  |     | NFC2/ | SCL     | G_RX | SCL  | G_RX  |
| D30   |          |      |      |       |     | D31   |         |      |      |       |
|       |          |      |      |       |     |       |         |      |      |       |
|       | Internal |      |      |       |     |       |         |      |      |       |
| D16   | SCL1     | SCL1 | SCL1 | SCL1  |     |       |        |      |      |       |
| D17   | SDA1     | SDA1 | SDA1 | SDA1  |     |       |        |      |      |       |

The default column shows the pin assignments for the Wio-SX1262 for XIAO
(standalone SKU 113010003 or nRF52840 kit SKU 102010710).
The I2C column shows an alternative pin assignment using I2C on D6/D7 in place of the GNSS.
The BTB column shows the pin assignment for the Wio-SX1262 -30-pin board-to-board connector version from the ESP32S3 kit.
The BLE-L column shows the pin assignment for the original DIY xiao_ble, and which is retained for legacy users.
Note that the in addition to the difference between the default and the I2C pinouts in placing the pins on NFC or
D6/D7, the user button is activated on D0. The button conflicts with the official GNSS module, so caution is advised.
*/

#define PINS_COUNT (33)
#define NUM_DIGITAL_PINS (33)
#define NUM_ANALOG_INPUTS (8)
#define NUM_ANALOG_OUTPUTS (0)

/*
 * Digital Pins
 */
#define D0 (0ul)
#define D1 (1ul)
#define D2 (2ul)
#define D3 (3ul)
#define D4 (4ul)
#define D5 (5ul)
#define D6 (6ul)
#define D7 (7ul)
#define D8 (8ul)
#define D9 (9ul)
#define D10 (10ul)

/*
 * Analog pins
 */
#define PIN_A0 (0)
#define PIN_A1 (1)
#define PIN_A2 (32)
#define PIN_A3 (3)
#define PIN_A4 (4)
#define PIN_A5 (5)
#define PIN_VBAT (32)
#define VBAT_ENABLE (14)

static const uint8_t A0 = PIN_A0;
static const uint8_t A1 = PIN_A1;
static const uint8_t A2 = PIN_A2;
static const uint8_t A3 = PIN_A3;
static const uint8_t A4 = PIN_A4;
static const uint8_t A5 = PIN_A5;
#define ADC_RESOLUTION 12

/*
 * LEDs
 */
#define LED_STATE_ON (0) // RGB LED is common anode
#define LED_RED (11)
#define LED_GREEN (13)
#define LED_BLUE (12)

#define PIN_LED1 LED_GREEN // PIN_LED1 is used in src/platform/nrf52/architecture.h to define LED_POWER
#define PIN_LED2 LED_BLUE
#define PIN_LED3 LED_RED

/*
 * Buttons
 */

/*
 * D0 is shared with PIN_GPS_STANDBY on the L76K GNSS Module, so refer to
 * GPS_L76K definition preventing this conflict
 */

// #define BUTTON_PIN D0

/*
 * Serial Interfaces
 */
#define PIN_SERIAL2_RX (-1)
#define PIN_SERIAL2_TX (-1)

/*
 * Pinout for SX126x
 */
#define USE_SX1262

#if defined(XIAO_BLE_LEGACY_PINOUT)
// Legacy xiao_ble variant pinout for third-party SX126x modules e.g. EBYTE E22
#define SX126X_CS D0
#define SX126X_DIO1 D1
#define SX126X_BUSY D2
#define SX126X_RESET D3
#define SX126X_RXEN D7
#else
#if defined(SEEED_XIAO_NRF_WIO_BTB)
#define SX126X_CS D3
#define SX126X_DIO1 D0
#define SX126X_BUSY D1
#define SX126X_RESET D2
#define SX126X_RXEN D4
#else
#define SX126X_CS D4
#define SX126X_DIO1 D1
#define SX126X_BUSY D3
#define SX126X_RESET D2
#define SX126X_RXEN D5
#endif // defined(SEEED_XIAO_NRF_WIO_BTB)
#endif // defined(XIAO_BLE_LEGACY_PINOUT)

#define SX126X_TXEN RADIOLIB_NC
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 1.8

#define SPI_INTERFACES_COUNT 1
#define PIN_SPI_MISO D9
#define PIN_SPI_MOSI D10
#define PIN_SPI_SCK D8

static const uint8_t SS = SX126X_CS;
static const uint8_t MOSI = PIN_SPI_MOSI;
static const uint8_t MISO = PIN_SPI_MISO;
static const uint8_t SCK = PIN_SPI_SCK;

/* GPS */
#if defined(TRAIL_COUNTER_SEN0171)
#define GPS_TX_PIN D6
#define GPS_RX_PIN D7
#define HAS_GPS 0
#else
#if defined(SEEED_XIAO_NRF_KIT_DEFAULT) || defined(SEEED_XIAO_NRF_WIO_BTB)
#define GPS_L76K
#define GPS_TX_PIN D6
#define GPS_RX_PIN D7
#if defined(SEEED_XIAO_NRF_KIT_DEFAULT)
#define PIN_GPS_STANDBY D0
#endif
#else
#define GPS_TX_PIN (30)
#define GPS_RX_PIN (31)
#endif
#define HAS_GPS 1
#endif // defined(TRAIL_COUNTER_SEN0171)

#define GPS_BAUDRATE 9600
#define GPS_THREAD_INTERVAL 50
#define PIN_SERIAL1_TX GPS_TX_PIN
#define PIN_SERIAL1_RX GPS_RX_PIN

/* Battery */
#define BATTERY_PIN PIN_VBAT
#define ADC_MULTIPLIER (3)
#define ADC_CTRL VBAT_ENABLE
#define ADC_CTRL_ENABLED LOW
#define EXT_CHRG_DETECT (23)
#define EXT_CHRG_DETECT_VALUE LOW
#define HICHG (22)
#define BATTERY_SENSE_RESOLUTION_BITS (10)

/* Wire Interfaces */
#define I2C_NO_RESCAN
#define WIRE_INTERFACES_COUNT 1

#if defined(XIAO_BLE_LEGACY_PINOUT)
#define PIN_WIRE_SDA D4
#define PIN_WIRE_SCL D5
#else
#if defined(SEEED_XIAO_NRF_KIT_DEFAULT) || defined(SEEED_XIAO_NRF_WIO_BTB)
#define PIN_WIRE_SDA 30
#define PIN_WIRE_SCL 31
#else
#define PIN_WIRE_SDA D6
#define PIN_WIRE_SCL D7
#endif
#endif

static const uint8_t SDA = PIN_WIRE_SDA;
static const uint8_t SCL = PIN_WIRE_SCL;

/* Buttons */
#if defined(SEEED_XIAO_NRF_KIT_I2C)
#define BUTTON_PIN D0
#endif

#if defined(SEEED_XIAO_NRF_WIO_BTB)
#define BUTTON_PIN D5
#endif

#ifdef __cplusplus
}
#endif

#endif
