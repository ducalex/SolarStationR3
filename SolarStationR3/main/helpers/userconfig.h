#include "ConfigProvider.h"

static char  STATION_NAME[64];               //
static int   STATION_POLL_INTERVAL;          // Seconds
static int   STATION_POWER_SAVING;           // Not used
static int   STATION_DISPLAY_TIMEOUT;        // Seconds

static char  WIFI_SSID[33];                  // 32 as per esp-idf
static char  WIFI_PASSWORD[65];              // 64 as per esp-idf
static int   WIFI_TIMEOUT;                   // Seconds

static char  HTTP_UPDATE_URL[128];           //
static char  HTTP_UPDATE_USERNAME[64];       //
static char  HTTP_UPDATE_PASSWORD[64];       //
static int   HTTP_UPDATE_INTERVAL;           // Seconds
static int   HTTP_UPDATE_TIMEOUT;            // Seconds

static double POWER_POLL_LOW_VBAT_TRESHOLD;  // Volts  (Maybe we should use percent so it works on any battery?)
static double POWER_HTTP_LOW_VBAT_TRESHOLD;  // Volts  (Maybe we should use percent so it works on any battery?)
static double POWER_VBAT_MULTIPLIER;         // Factor (If there is a voltage divider)

static ConfigProvider config;

void saveConfiguration(bool update_only)
{
    config.setString("station.name", STATION_NAME);
    config.setInteger("station.poll_interval", STATION_POLL_INTERVAL);
    config.setInteger("station.power_saving", STATION_POWER_SAVING);
    config.setInteger("station.display_timeout", STATION_DISPLAY_TIMEOUT);

    config.setString("wifi.ssid", WIFI_SSID);
    config.setString("wifi.password", WIFI_PASSWORD);
    config.setInteger("wifi.timeout", WIFI_TIMEOUT);

    config.setString("http.update_url", HTTP_UPDATE_URL);
    config.setString("http.update_username", HTTP_UPDATE_USERNAME);
    config.setString("http.update_password", HTTP_UPDATE_PASSWORD);
    config.setInteger("http.update_timeout", HTTP_UPDATE_TIMEOUT);
    config.setInteger("http.update_interval", HTTP_UPDATE_INTERVAL);

    config.setDouble("power.http_low_battery_treshold", POWER_POLL_LOW_VBAT_TRESHOLD);
    config.setDouble("power.poll_low_battery_treshold", POWER_HTTP_LOW_VBAT_TRESHOLD);
    config.setDouble("power.vbat_multiplier", POWER_VBAT_MULTIPLIER);

    #ifdef CONFIG_USE_FILE
        config.saveFile(CONFIG_USE_FILE, update_only);
    #endif
    #ifdef CONFIG_USE_NVS
        config.saveNVS(CONFIG_USE_NVS, update_only);
    #endif
}

void loadConfiguration()
{
    #ifdef CONFIG_USE_FILE
    if (!config.loadFile(CONFIG_USE_FILE))
    #endif
    {
    #ifdef CONFIG_USE_NVS
        config.loadNVS(CONFIG_USE_NVS);
    #endif
    }

    // cJSON has a weird handling of NULL, it's better to pass an empty string if we
    // want keys to appear in the config file
    config.getString("station.name", (char*)DEFAULT_STATION_NAME, STATION_NAME);
    config.getInteger("station.poll_interval", DEFAULT_STATION_POLL_INTERVAL, &STATION_POLL_INTERVAL);
    config.getInteger("station.power_saving", DEFAULT_STATION_POWER_SAVING, &STATION_POWER_SAVING);
    config.getInteger("station.display_timeout", DEFAULT_STATION_DISPLAY_TIMEOUT, &STATION_DISPLAY_TIMEOUT);

    config.getString("wifi.ssid", (char*)DEFAULT_WIFI_SSID, WIFI_SSID);
    config.getString("wifi.password", (char*)DEFAULT_WIFI_PASSWORD, WIFI_PASSWORD);
    config.getInteger("wifi.timeout", DEFAULT_WIFI_TIMEOUT, &WIFI_TIMEOUT);

    config.getString("http.update_url", (char*)DEFAULT_HTTP_UPDATE_URL, HTTP_UPDATE_URL);
    config.getString("http.update_username", (char*)DEFAULT_HTTP_UPDATE_USERNAME, HTTP_UPDATE_USERNAME);
    config.getString("http.update_password", (char*)DEFAULT_HTTP_UPDATE_PASSWORD, HTTP_UPDATE_PASSWORD);
    config.getInteger("http.update_timeout", DEFAULT_HTTP_UPDATE_TIMEOUT, &HTTP_UPDATE_TIMEOUT);
    config.getInteger("http.update_interval", DEFAULT_HTTP_UPDATE_INTERVAL, &HTTP_UPDATE_INTERVAL);

    config.getDouble("power.http_low_battery_treshold", DEFAULT_POWER_POLL_LOW_VBAT_TRESHOLD, &POWER_POLL_LOW_VBAT_TRESHOLD);
    config.getDouble("power.poll_low_battery_treshold", DEFAULT_POWER_HTTP_LOW_VBAT_TRESHOLD, &POWER_HTTP_LOW_VBAT_TRESHOLD);
    config.getDouble("power.vbat_multiplier", DEFAULT_POWER_VBAT_MULTIPLIER, &POWER_VBAT_MULTIPLIER);

    saveConfiguration(true);
}

// CONFIG_STR(STATION_NAME,                   "station.name",                      "SolarStationR3"),
// CONFIG_INT(STATION_POLL_INTERVAL,          "station.poll_interval",             60),
// CONFIG_INT(STATION_POWER_SAVING,           "station.power_saving",              0 ),
// CONFIG_INT(STATION_DISPLAY_TIMEOUT,        "station.display_timeout",           10),
// CONFIG_STR(WIFI_SSID,                      "wifi.ssid",                         ""),
// CONFIG_STR(WIFI_PASSWORD,                  "wifi.password",                     ""),
// CONFIG_INT(WIFI_TIMEOUT,                   "wifi.timeout",                      0 ),
// CONFIG_STR(HTTP_UPDATE_URL,                "http.update_url",                   ""),
// CONFIG_STR(HTTP_UPDATE_USERNAME,           "http.update_username",              ""),
// CONFIG_STR(HTTP_UPDATE_PASSWORD,           "http.update_password",              ""),
// CONFIG_INT(HTTP_UPDATE_INTERVAL,           "http.update_timeout",               60),
// CONFIG_INT(HTTP_UPDATE_TIMEOUT,            "http.update_interval",              30),
// CONFIG_DBL(POWER_POLL_LOW_VBAT_TRESHOLD,   "power.http_low_battery_treshold",   3.7),
// CONFIG_DBL(POWER_HTTP_LOW_VBAT_TRESHOLD,   "power.poll_low_battery_treshold",   3.5),
// CONFIG_DBL(POWER_VBAT_MULTIPLIER,          "power.vbat_multiplier",             2.0),