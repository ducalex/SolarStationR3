#include "ConfigProvider.h"

static const char *CONFIG_USE_FILE = "/sd/config.json";
static const char *CONFIG_USE_NVS = "configuration";

static ConfigProvider config;
// Macros to get values
#define CFG_STR(key) config.getString(key, (char*)"")
#define CFG_INT(key) config.getInteger(key, 0)
#define CFG_DBL(key) config.getDouble(key, 0)
// Macros to set values
#define CFG_SET_STR(key, value) config.setString(key, (char*)value);
#define CFG_SET_INT(key, value) config.setInteger(key, value)
#define CFG_SET_DBL(key, value) config.setDouble(key, value)
// Macros to load default value if they key doesn't exist
#define CFG_LOAD_STR(key, _default) config.setString(key, config.getString(key, (char*)_default));
#define CFG_LOAD_INT(key, _default) config.setInteger(key, config.getInteger(key, _default))
#define CFG_LOAD_DBL(key, _default) config.setDouble(key, config.getDouble(key, _default))

void saveConfiguration(bool update_only)
{
    if (SD.cardType() != CARD_NONE) {
        config.saveFile(CONFIG_USE_FILE, update_only);
    }
    config.saveNVS(CONFIG_USE_NVS, update_only);
}

void loadConfiguration()
{
    if (SD.cardType() == CARD_NONE || !config.loadFile(CONFIG_USE_FILE)) {
        config.loadNVS(CONFIG_USE_NVS);
    }

    CFG_LOAD_STR("station.name", DEFAULT_STATION_NAME);
    CFG_LOAD_INT("station.poll_interval", DEFAULT_STATION_POLL_INTERVAL);
    CFG_LOAD_INT("station.display_timeout", DEFAULT_STATION_DISPLAY_TIMEOUT);

    CFG_LOAD_STR("wifi.ssid", DEFAULT_WIFI_SSID);
    CFG_LOAD_STR("wifi.password", DEFAULT_WIFI_PASSWORD);
    CFG_LOAD_INT("wifi.timeout", DEFAULT_WIFI_TIMEOUT);

    CFG_LOAD_STR("http.update.url", DEFAULT_HTTP_UPDATE_URL);
    CFG_LOAD_STR("http.update.username", DEFAULT_HTTP_UPDATE_USERNAME);
    CFG_LOAD_STR("http.update.password", DEFAULT_HTTP_UPDATE_PASSWORD);
    CFG_LOAD_INT("http.update.interval", DEFAULT_HTTP_UPDATE_INTERVAL);
    CFG_LOAD_INT("http.ota.enabled", 1);
    CFG_LOAD_INT("http.timeout", DEFAULT_HTTP_TIMEOUT);

    CFG_LOAD_DBL("power.power_save_strategy", DEFAULT_POWER_POWER_SAVE_STRATEGY);
    CFG_LOAD_DBL("power.poll_low_vbat_treshold", DEFAULT_POWER_POLL_LOW_VBAT_TRESHOLD);
    CFG_LOAD_DBL("power.http_low_vbat_treshold", DEFAULT_POWER_HTTP_LOW_VBAT_TRESHOLD);
    CFG_LOAD_DBL("power.vbat_multiplier", DEFAULT_POWER_VBAT_MULTIPLIER);

    saveConfiguration(true);
}
