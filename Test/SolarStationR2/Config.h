#ifndef SOLARSTATION_CONFIG_H
#define SOLARSTATION_CONFIG_H

// Battery
#define VBAT_MULTIPLIER 2.0 // If there is a voltage divider

// Power management
#define POWERMNG_EMERGENCY_POWER_VOLT 3.75d

// Wifi
const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const int   WIFI_CONNECTIONTIMEOUTMS = 10000; // in milliseconds

// HTTP
const int   HTTP_REQUEST_TIMEOUT_MS = 5*1000;

const char* HTTP_UPDATE_URL = "http://192.168.5.214";
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

#endif
