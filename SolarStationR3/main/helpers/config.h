#include "ConfigProvider.h"

static char  STATION_NAME[64]         = "SolarStationR3";
static int   STATION_POLL_INTERVAL    = 60;  // Seconds
static int   STATION_POWER_SAVING     = 0;   // Not used yet
static int   STATION_DISPLAY_TIMEOUT  = 10;  // Seconds

static char  WIFI_SSID[33]            = "";  // 32 per esp-idf
static char  WIFI_PASSWORD[65]        = "";  // 64 per esp-idf
static int   WIFI_TIMEOUT             = 10;  // Seconds

static char  HTTP_UPDATE_URL[128]     = "";
static char  HTTP_UPDATE_USERNAME[64] = "";
static char  HTTP_UPDATE_PASSWORD[64] = "";
static int   HTTP_UPDATE_INTERVAL     = 60;  // Seconds
static int   HTTP_UPDATE_TIMEOUT      = 30;  // Seconds

static double POWER_POLL_LOW_VBAT_TRESHOLD = 3.7;  // Volts  (Maybe we should use percent so it works on any battery?)
static double POWER_HTTP_LOW_VBAT_TRESHOLD = 3.5;  // Volts  (Maybe we should use percent so it works on any battery?)
static double POWER_VBAT_MULTIPLIER        = 2.0;  // Factor (If there is a voltage divider)

static const char *CONFIG_FILE = "/sd/config.json";
static const char *CONFIG_NVS_NS = "configuration";

static ConfigProvider config;

void saveConfiguration(bool update_only)
{
    config.setString("station.name", STATION_NAME);
    config.setInteger("station.poll_interval", STATION_POLL_INTERVAL);
    config.setInteger("station.power_saving", 0);
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

    config.saveFile(CONFIG_FILE, update_only);
    config.saveNVS(CONFIG_NVS_NS, update_only);
}

void loadConfiguration()
{
    if (!config.loadFile(CONFIG_FILE)) {
        config.loadNVS(CONFIG_NVS_NS);
    }

    // cJSON has a weird handling of NULL, it's better to pass an empty string if we
    // want keys to appear in the config file
    config.getString("station.name", STATION_NAME, STATION_NAME);
    config.getInteger("station.poll_interval", STATION_POLL_INTERVAL, &STATION_POLL_INTERVAL);
    config.getInteger("station.power_saving", STATION_POWER_SAVING, &STATION_POWER_SAVING);
    config.getInteger("station.display_timeout", STATION_DISPLAY_TIMEOUT, &STATION_DISPLAY_TIMEOUT);

    config.getString("wifi.ssid", WIFI_SSID, WIFI_SSID);
    config.getString("wifi.password", WIFI_PASSWORD, WIFI_PASSWORD);
    config.getInteger("wifi.timeout", WIFI_TIMEOUT, &WIFI_TIMEOUT);

    config.getString("http.update_url", HTTP_UPDATE_URL, HTTP_UPDATE_URL);
    config.getString("http.update_username", HTTP_UPDATE_USERNAME, HTTP_UPDATE_USERNAME);
    config.getString("http.update_password", HTTP_UPDATE_PASSWORD, HTTP_UPDATE_PASSWORD);
    config.getInteger("http.update_timeout", HTTP_UPDATE_TIMEOUT, &HTTP_UPDATE_TIMEOUT);
    config.getInteger("http.update_interval", HTTP_UPDATE_INTERVAL, &HTTP_UPDATE_INTERVAL);

    config.getDouble("power.http_low_battery_treshold", POWER_POLL_LOW_VBAT_TRESHOLD, &POWER_POLL_LOW_VBAT_TRESHOLD);
    config.getDouble("power.poll_low_battery_treshold", POWER_HTTP_LOW_VBAT_TRESHOLD, &POWER_HTTP_LOW_VBAT_TRESHOLD);
    config.getDouble("power.vbat_multiplier", POWER_VBAT_MULTIPLIER, &POWER_VBAT_MULTIPLIER);

    saveConfiguration(true);
}
