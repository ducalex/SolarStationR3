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
