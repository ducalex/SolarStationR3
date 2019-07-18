#define PROJECT_VERSION "1.0-dev"

#define FACTORY_RESET_PIN 15 // Just a reminder to not use this pin
#define WAKEUP_BUTTON_PIN 15 // Wake from deep sleep. Might use touch if it's less power-hungry

// This pin can drive peripherals directly (up to 40mA) or can drive a transistor
// Note: It seems to drop ~0.12V from VCC, which makes some sensors and SD Card not work when VCC is 3.3 :(
#define PERIPH_POWER_PIN 16
#define PERIPH_POWER_PIN_LEVEL HIGH

// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define OLED_I2C_ADDRESS 0x3C

// SPI
#define SD_PIN_NUM_MISO 19
#define SD_PIN_NUM_MOSI 23
#define SD_PIN_NUM_CLK  18
#define SD_PIN_NUM_CS 5

// DHT
#define DHT_PIN 32
#define DHT_TYPE DHT22   // or DHT11

// ANEMOMETER
#define ANEMOMETER_PIN 33

// If you rely on those settings don't forget to make erase_flash
// Otherwise the NVS will have priority
#define DEFAULT_STATION_NAME     "SolarStationR3" // 64
#define DEFAULT_STATION_POLL_INTERVAL         60  // Seconds
#define DEFAULT_STATION_POWER_SAVING          0   // Not used yet
#define DEFAULT_STATION_DISPLAY_TIMEOUT       10  // Seconds
#define DEFAULT_WIFI_SSID                     ""  // 32 per esp-idf
#define DEFAULT_WIFI_PASSWORD                 ""  // 64 per esp-idf
#define DEFAULT_WIFI_TIMEOUT                  10  // Seconds
#define DEFAULT_HTTP_UPDATE_URL               ""  // 128
#define DEFAULT_HTTP_UPDATE_USERNAME          ""  // 64
#define DEFAULT_HTTP_UPDATE_PASSWORD          ""  // 64
#define DEFAULT_HTTP_UPDATE_INTERVAL          300 // Seconds
#define DEFAULT_HTTP_UPDATE_TIMEOUT           30  // Seconds
#define DEFAULT_POWER_POLL_LOW_VBAT_TRESHOLD  3.7 // Volts  (Maybe we should use percent so it works on any battery?)
#define DEFAULT_POWER_HTTP_LOW_VBAT_TRESHOLD  3.5 // Volts  (Maybe we should use percent so it works on any battery?)
#define DEFAULT_POWER_VBAT_MULTIPLIER         2.0 // Factor (If there is a voltage divider)
