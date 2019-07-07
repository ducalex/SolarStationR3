#define PROJECT_VER "1"

#define FACTORY_RESET_PIN 15 // Just a reminder to not use this pin

// These pins are put in parallel to power all our peripherals (Sensors, Screen, SD Card).
// A single pin might be enough, the datasheet isn't clear about the max current available.
#define PERIPH_POWER_PIN_1 16
#define PERIPH_POWER_PIN_2 16

// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// SPI
#define SD_PIN_NUM_MISO 19
#define SD_PIN_NUM_MOSI 23
#define SD_PIN_NUM_CLK  18
#define SD_PIN_NUM_CS 5

// DHT
#define DHT_PIN 32
#define DHT_TYPE DHT22   // or DHT11

#define USE_OLED 1