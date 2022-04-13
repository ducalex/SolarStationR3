#define PROJECT_VERSION "1.0-dev"

#define FACTORY_RESET_PIN 15 // Just a reminder to not use this pin
#define ACTION_BUTTON_PIN 13

// This pin can drive peripherals directly (up to 40mA) or can drive a transistor
// Note: It seems to drop ~0.12V from VCC, which makes some sensors and SD Card not work when VCC is 3.3 :(
#define PERIPH_POWER_PIN 16
#define PERIPH_POWER_PIN_LEVEL HIGH

// I2C
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22
#define OLED_I2C_ADDRESS 0x3C

// DHT
#define DHT_PIN 32
#define DHT_TYPE DHT22   // or DHT11

// ANEMOMETER
#define ANEMOMETER_PIN 33

// NTP
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.nist.gov"

// If you rely on those settings don't forget to make erase_flash
// Otherwise the NVS will have priority
#define DEFAULT_STATION_NAME                   "SolarStationR3"  // Used as device ID by InfluxDB and SQL, and as group by Adafruit.io
#define DEFAULT_STATION_GROUP                  "weather"         // Used as measurement prefix in InfluxDB and table prefix in SQL
#define DEFAULT_STATION_POLL_INTERVAL          60                // Seconds
#define DEFAULT_STATION_SLEEP_DELAY            10                // Seconds
#define DEFAULT_STATION_DISPLAY_CONTENT        "Volt: $bat.2 $sol.2\n"   \
                                               "Light: $l1.0 $l2.0\n"    \
                                               "Temp: $t1.2 $t2.2\n"     \
                                               "Humidity: $h1.0 $h2.0\n" \
                                               "Pres.: $p1.2 $p2.2\n"    \
                                               "Wind: $ws.2 $wd.0"
#define DEFAULT_WIFI_SSID                      ""      // 32 per esp-idf
#define DEFAULT_WIFI_PASSWORD                  ""      // 64 per esp-idf
#define DEFAULT_WIFI_TIMEOUT                   30      // Seconds
#define DEFAULT_HTTP_UPDATE_URL                ""      // 128
#define DEFAULT_HTTP_UPDATE_TYPE               "JSON"  // JSON or InfluxDB
#define DEFAULT_HTTP_UPDATE_USERNAME           ""      // 64
#define DEFAULT_HTTP_UPDATE_PASSWORD           ""      // 64
#define DEFAULT_HTTP_UPDATE_DATABASE           ""      // Only InfluxDB uses this for now
#define DEFAULT_HTTP_UPDATE_INTERVAL           300     // Seconds
#define DEFAULT_HTTP_TIMEOUT                   30      // Seconds
#define DEFAULT_POWER_SAVE_STRATEGY            0       // Not used yet
#define DEFAULT_POWER_SAVE_TRESHOLD            3.6     // Volts  (Maybe we should use percent so it works on any battery?)
#define DEFAULT_SENSORS_ADC_MULTIPLIER         2.0     // Factor (If there is a voltage divider)
#define DEFAULT_SENSORS_ANEMOMETER_RADIUS      15      // centimeters
#define DEFAULT_SENSORS_ANEMOMETER_CALIBRATION 1       // The calculated value is multiplied by this

// ConfigProvider settings
#define CONFIG_USE_FILE "/sd/config.json"
#define CONFIG_USE_NVS "configuration"
