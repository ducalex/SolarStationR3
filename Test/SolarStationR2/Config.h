#ifndef SOLARSTATION_CONFIG_H
#define SOLARSTATION_CONFIG_H

// Battery
#define VBAT_MULTIPLIER 2.0 // If there is a voltage divider

// Power management
#define POWERMNG_EMERGENCY_POWER_VOLT_MIN 3.58d
#define POWERMNG_EMERGENCY_POWER_VOLT_BOOT ((POWERMNG_EMERGENCY_POWER_VOLT_MIN+POWERMNG_EMERGENCY_POWER_VOLT_MAX)/2.0d)
#define POWERMNG_EMERGENCY_POWER_VOLT_MAX 3.7d

// Wifi
const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const int   WIFI_CONNECTIONTIMEOUTMS = 10000; // in milliseconds

// HTTP
const int   HTTP_REQUEST_TIMEOUT_MS = 5*1000;

const char* HTTP_UPDATE_URL = "http://192.168.5.214/Main/PostData";
const int   HTTP_UPDATE_PORT = 9999;

// Screen
#define SCREEN_OLED_I2CADDRESS 0x3C

// DHT
#define DHTPIN 15     // what pin we're connected to
#define DHTTYPE DHT11   // DHT 22  (AM2302)

// ADC
#define VADC_SENSORGAIN GAIN_ONE
#define VADC_PERBIT 0.000125d
#define VADC_INPUT_LIGHTSENSOR 1
#define VADC_INPUT_BATTERY 3

// Measurement
#define MEASUREMENTAVGCOUNT 60

#endif
