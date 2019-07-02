#include "ConfigProvider.h"

static char *STATION_NAME;         // 32
static char *WIFI_SSID;            // 32
static char *WIFI_PASSWORD;        // 64
static char *HTTP_UPDATE_URL;      // 128
static char *HTTP_UPDATE_USERNAME; // 64
static char *HTTP_UPDATE_PASSWORD; // 64
static int   HTTP_UPDATE_TIMEOUT;  // Seconds
static int   POLL_INTERVAL;        // Seconds
static int   POLL_METHOD;          //

static const char *CONFIG_FILE = "/sd/config.json";
ConfigProvider config;

void loadConfiguration()
{
    if (config.loadFile(CONFIG_FILE)) {
        config.saveNVS("configuration");
    } else {
        config.loadNVS("configuration");
    }

    STATION_NAME         = config.getString("station.name", (char *)"SolarStationR3");
    WIFI_SSID            = config.getString("wifi.ssid", NULL);
    WIFI_PASSWORD        = config.getString("wifi.password", NULL);
    HTTP_UPDATE_URL      = config.getString("http.update_url", NULL);
    HTTP_UPDATE_USERNAME = config.getString("http.update_username", NULL);
    HTTP_UPDATE_PASSWORD = config.getString("http.update_password", NULL);
    HTTP_UPDATE_TIMEOUT  = config.getInteger("http.update_timeout", 30);
    POLL_INTERVAL        = config.getInteger("poll.interval", 30);
    POLL_METHOD          = config.getInteger("poll.method", 0);
}


void saveConfiguration()
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

    config.saveFile(CONFIG_FILE);
    config.saveNVS("configuration");
}
