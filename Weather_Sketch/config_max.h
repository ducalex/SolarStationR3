#ifndef WEATHERSTATION_CONFIG
//#define WEATHERSTATION_CONFIG

#define VBAT_PIN 36 // GPIO. The cal code assumes it's on ADC1
#define VBAT_OFFSET 0.0 // If there is a diode or transistor in the way
#define VBAT_MULTIPLIER 2 // If there is a voltage divider
#define OLED_ADDRESS 0x3C // Usually 0x3C or 0x3D
#define DHT_PIN 19 // comment to use DHT11 instead
#define DHT_TYPE SimpleDHT22 // or SimpleDHT11

#define STATION_NAME "PATATE"

#define LIGHTSLEEPMODE
// #define WITHOLED

const char* WIFI_SSID = "Maxou_TestIOT";
const char* WIFI_PASSWORD = "!UnitedStatesOfSmash97!";
const char* HTTP_UPDATE_URL = "http://192.168.5.214";
const int   HTTP_UPDATE_PORT = 9999;
const char* HTTP_USERNAME = "";
const char* HTTP_PASSWORD = "";
const char* STATION_ID = "";
const char* NORTH_OFFSET = "";
const int   POLL_INTERVAL = 30; // in seconds
const int   HTTP_REQUEST_TIMEOUT_MS = 5*1000;

#endif
