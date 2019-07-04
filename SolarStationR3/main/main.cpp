#include "Arduino.h"
#include "WiFi.h"
#include "ConfigProvider.h"
#include "DHT.h"
#include "SD.h"
#include "esp_log.h"
#include "esp_pm.h"

#include "config.h"
#include "helpers/display.h"
#include "helpers/config.h"
#include "helpers/time.h"
#include "helpers/avgr.h"

RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static uint32_t boot_time = 0;
RTC_DATA_ATTR static uint32_t start_time = 0;

#define _debug ESP_ERROR_CHECK_WITHOUT_ABORT
#define PRINT_MEMORY_STATS() { \
  multi_heap_info_t info; \
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT); \
  ESP_LOGI("Memory", "Memory: Used: %d KB   Free: %d KB", \
        info.total_allocated_bytes / 1024, info.total_free_bytes / 1024); }


static void hibernate()
{
    // Cleanup
    Display.end();
    SD.end();

    // Sleep
    int interval_ms = POLL_INTERVAL * 1000;
    int sleep_time = interval_ms - (rtc_millis() - start_time);

    if (sleep_time < 100) {
        sleep_time = interval_ms > 100 ? interval_ms : 100;
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
    }

    ESP_LOGI(__func__, "Sleeping for %dms", sleep_time);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    //esp_light_sleep_start();
    esp_deep_sleep_start();
}


static void readSensors()
{
    float t, h, p;
    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    }
}


void setup()
{
    printf("\n################### WEATHER STATION (Version: %s) ###################\n\n", PROJECT_VER);
    ESP_LOGI("Uptime", "Uptime: %d seconds (Cycles: %d)", (rtc_millis() - boot_time) / 1000, wake_count);
    PRINT_MEMORY_STATS();

    if (boot_time == 0) {
        boot_time = rtc_millis();
    }

    start_time = rtc_millis();
    wake_count++;

    /* This seems to interfer with Arduino I2C
    esp_pm_config_esp32_t pm_config;
        pm_config.max_freq_mhz = 240;
        pm_config.min_freq_mhz = 240; // 40
        pm_config.light_sleep_enable = true;
    esp_pm_configure(&pm_config);
    */

    // Reduce verbosity of some logs
    esp_log_level_set("wifi", ESP_LOG_WARN);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Display.begin();

    if (SD.begin()) {
        ESP_LOGI(__func__, "SD Card successfully mounted.");
    } else {
        ESP_LOGW(__func__, "Failed to mount SD Card.");
    }

    loadConfiguration();

    ESP_LOGI(__func__, "Station name: %s", STATION_NAME);
    Display.printf("# %s #\n", STATION_NAME);
}


void loop()
{
    bool wifi_available = strlen(WIFI_SSID) > 0;

    PRINT_MEMORY_STATS();

    if (wifi_available) {
        ESP_LOGI(__func__, "WiFi: Connecting to: '%s'...", WIFI_SSID);
        Display.printf("Connecting to %s...", WIFI_SSID);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        WiFi.setSleep(true);

        // Read sensors while wifi connects
        readSensors();

        while (WiFi.status() != WL_CONNECTED && (rtc_millis() - start_time) < (POLL_INTERVAL * 1000)) {
            delay(500);
            Display.printf(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(__func__, "WiFi: Connected to: '%s' with IP %s", WIFI_SSID, WiFi.localIP().toString().c_str());
            Display.printf("\nConnected!\nIP: %s", WiFi.localIP().toString().c_str());

            // Now do the HTTP Request
        }
        else {
            ESP_LOGW(__func__, "WiFi: Failed to connect to: '%s'", WIFI_SSID);
            Display.printf("\nFailed!");
        }
        PRINT_MEMORY_STATS();

        WiFi.disconnect(true);
    }
    else {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("WiFi disabled!\n");
        readSensors();
    }

    // Fill the screen with sensor info and keep it on for a few seconds
    delay(DISPLAY_TIMEOUT * 1000);

    // Sleep
    hibernate();
}
