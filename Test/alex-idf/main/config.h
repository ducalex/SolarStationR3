#pragma once

#define FACTORY_RESET_PIN 15 // Just a reminder to not use this pin

// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// SPI
#define SD_PIN_NUM_MISO 19
#define SD_PIN_NUM_MOSI 23
#define SD_PIN_NUM_CLK  18
#define SD_PIN_NUM_CS 5

//#define LOG_TO_SDCARD 1


#define BMPSENSORACTIVE 0

// Battery
#define VBAT_MULTIPLIER 2.0 // If there is a voltage divider

// Power management
#define POWERMNG_EMERGENCY_POWER_VOLT_MIN 3.90d
#define POWERMNG_EMERGENCY_POWER_VOLT_BOOT ((POWERMNG_EMERGENCY_POWER_VOLT_MIN+POWERMNG_EMERGENCY_POWER_VOLT_MAX)/2.0d)
#define POWERMNG_EMERGENCY_POWER_VOLT_MAX 4.0d

// Screen
#define SCREEN_OLED_I2CADDRESS 0x3C

// DHT
#define DHT_PIN 32     // what pin we're connected to
#define DHT_TYPE DHT22   // DHT 22  (AM2302)

// ADC
#define VADC_SENSORGAIN GAIN_ONE
#define VADC_PERBIT 0.000125d
#define VADC_INPUT_LIGHTSENSOR 1
#define VADC_INPUT_BATTERY 3

// BMP085
#define BMPTEMPC_OFFSET -2.5d

#ifndef PROJECT_VER
#define PROJECT_VER "0"
#endif
