#include "ConfigProvider.h"

static char  STATION_NAME[64];          // 32
static char  WIFI_SSID[64];             // 32
static char  WIFI_PASSWORD[64];         // 64
static char  HTTP_UPDATE_URL[128];      // 128
static char  HTTP_UPDATE_USERNAME[64];  // 64
static char  HTTP_UPDATE_PASSWORD[64];  // 64
static int   HTTP_UPDATE_TIMEOUT;       // Seconds
static int   POLL_INTERVAL;             // Seconds
static int   POLL_METHOD;               //
static int   DISPLAY_TIMEOUT;           //

static const char *CONFIG_FILE = "/sd/config.json";
static const char *CONFIG_NVS_NS = "configuration";

static ConfigProvider config;

void saveConfiguration(bool update_only)
{
    config.setString("station.name", STATION_NAME);
    config.setString("wifi.ssid", WIFI_SSID);
    config.setString("wifi.password", WIFI_PASSWORD);
    config.setString("http.update_url", HTTP_UPDATE_URL);
    config.setString("http.update_username", HTTP_UPDATE_USERNAME);
    config.setString("http.update_password", HTTP_UPDATE_PASSWORD);
    config.setInteger("http.update_timeout", HTTP_UPDATE_TIMEOUT);
    config.setInteger("poll.interval", POLL_INTERVAL);
    config.setInteger("poll.method", POLL_METHOD);
    config.setInteger("display.timeout", DISPLAY_TIMEOUT);

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
    config.getString("station.name", (char*)"SolarStationR3", STATION_NAME);
    config.getString("wifi.ssid", (char*)"", WIFI_SSID);
    config.getString("wifi.password", (char*)"", WIFI_PASSWORD);
    config.getString("http.update_url", (char*)"", HTTP_UPDATE_URL);
    config.getString("http.update_username", (char*)"", HTTP_UPDATE_USERNAME);
    config.getString("http.update_password", (char*)"", HTTP_UPDATE_PASSWORD);
    config.getInteger("http.update_timeout", 30, &HTTP_UPDATE_TIMEOUT);
    config.getInteger("poll.interval", 60, &POLL_INTERVAL);
    config.getInteger("poll.method", 0, &POLL_METHOD);
    config.getInteger("display.timeout", 5, &DISPLAY_TIMEOUT);

    saveConfiguration(true);
}
