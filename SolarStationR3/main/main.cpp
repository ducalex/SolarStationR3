#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "ConfigProvider.h"
#include "SD.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "config.h"
// Most of these should have their own .cpp file, but I'm lazy :(
#include "helpers/config.h"
#include "helpers/display.h"
#include "helpers/sensors.h"
#include "helpers/time.h"

RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static uint32_t boot_time = 0;
RTC_DATA_ATTR static uint32_t start_time = 0;
RTC_DATA_ATTR static uint32_t last_http_update = 0;

#define ls_delay(ms) esp_sleep_enable_timer_wakeup((ms) * 1000); esp_light_sleep_start();
#define PRINT_MEMORY_STATS() { \
  multi_heap_info_t info; \
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT); \
  ESP_LOGI("Memory", "Memory: Used: %d KB   Free: %d KB", \
        info.total_allocated_bytes / 1024, info.total_free_bytes / 1024); }


static void hibernate()
{
    PRINT_MEMORY_STATS();

    // Cleanup
    Display.end();
    SD.end();

    // Power down our peripherals
    pinMode(PERIPH_POWER_PIN_1, INPUT_PULLDOWN);
    pinMode(PERIPH_POWER_PIN_2, INPUT_PULLDOWN);

    // Sleep
    int interval_ms = STATION_POLL_INTERVAL * 1000;
    int sleep_time = interval_ms - millis();

    if (sleep_time < 1000) {
        sleep_time = interval_ms > 1000 ? interval_ms : 1000;
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
    }

    ESP_LOGI(__func__, "Sleeping for %dms", sleep_time);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    esp_deep_sleep_start();
}


static void firmware_upgrade(const char *file)
{
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    const esp_partition_t *current = esp_ota_get_running_partition();

    if (current->address != factory->address) {
        // As we have a single OTA, we can only flash from factory. We reboot to it.
        ESP_LOGI(__func__, "Rebooting to factory image");
        esp_ota_set_boot_partition(factory);
        esp_restart();
    }

    // Danger zone
    assert(target != NULL);

    ESP_LOGI(__func__, "Flashing file %s to 0x%x", file, target->address);

    FILE *fp = fopen(file, "r+");
    if (fp == NULL) {
        ESP_LOGE(__func__, "Unable to open file!");
        return;
    }

    Display.clear();
    Display.printf("Upgrading firmware...\n");

    void *buffer = malloc(16 * 1024);
    esp_ota_handle_t ota;
    esp_err_t err;
    size_t size, count = 0;

    err = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota);
    if (err != ESP_OK) goto finish;

    while ((size = fread(buffer, 1, 16 * 1024, fp)) > 0) {
        err = esp_ota_write(ota, buffer, size);
        if (err != ESP_OK) goto finish;
        Display.printf(count++ % 22 ? "." : "\n");
    }

    err = esp_ota_end(ota);
    if (err != ESP_OK) goto finish;

    err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK) goto finish;

  finish:
    fclose(fp);
    free(buffer);
    if (err == ESP_OK) {
        // Rename file after successful install
        char new_name[64];
        strcpy(new_name, file);
        new_name[strlen(new_name)-4] = 0;
        strcat(new_name, "_installed.bin");
        unlink(new_name);
        rename(file, new_name);

        Display.printf("\n\nComplete!\n\nPlease press reset.");
        ESP_LOGI(__func__, "Firmware successfully flashed!");
    } else {
        ESP_LOGE(__func__, "Firmware upgrade failed: %s", esp_err_to_name(err));
        Display.printf("\n\nFailed!\n\n%s\nPlease press reset.", esp_err_to_name(err));
    }
    vTaskDelete(NULL);
}


static void httpRequest()
{
    ESP_LOGI(__func__, "HTTP: POST request to '%s'...", HTTP_UPDATE_URL);
    Display.printf("HTTP: POST...");

    HTTPClient http;

    http.begin(HTTP_UPDATE_URL);
    http.setTimeout(HTTP_UPDATE_TIMEOUT * 1000);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    if (strlen(HTTP_UPDATE_USERNAME) > 0) {
        http.setAuthorization(HTTP_UPDATE_USERNAME, HTTP_UPDATE_PASSWORD);
    }

    char payload[512];
    char *sensors_data = serializeSensors();

    sprintf(payload, "station=%s&ps=%d&uptime=%d&cycles=%d" "&%s",
        STATION_NAME, 0, (rtc_millis() - boot_time) / 1000, wake_count, sensors_data);

    free(sensors_data);

    ESP_LOGI(__func__, "HTTP: Sending: '%s'", payload);

    int httpCode = http.POST(payload);
    ESP_LOGI(__func__, "HTTP: Return code: %d", httpCode);

    if (httpCode > 0) {
        ESP_LOGI(__func__, "HTTP: Received: '%s'", http.getString().c_str());
        Display.printf("%d\n", httpCode);
    } else {
        Display.printf("error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    //last_http_update = rtc_millis();
    last_http_update = start_time;
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

    // Power up our peripherals (Do not switch both pin to output at once!)
    pinMode(PERIPH_POWER_PIN_1, OUTPUT);
    digitalWrite(PERIPH_POWER_PIN_1, HIGH);
    pinMode(PERIPH_POWER_PIN_2, OUTPUT);
    digitalWrite(PERIPH_POWER_PIN_2, HIGH);
    delay(10); // Wait for peripherals to stabilize

    // Causes random crashes with wifi (once in every 50 boots or so)
    // esp_pm_config_esp32_t pm_config;
    //     pm_config.max_freq_mhz = 160;
    //     pm_config.min_freq_mhz = 160;
    //     pm_config.light_sleep_enable = true;
    // esp_pm_configure(&pm_config);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Display.begin();

    if (SD.begin()) {
        ESP_LOGI(__func__, "SD Card successfully mounted.");
    } else {
        ESP_LOGW(__func__, "Failed to mount SD Card.");
    }

    loadConfiguration();

    ESP_LOGI(__func__, "Station name: %s", STATION_NAME);
    Display.printf("# %s #\n\n", STATION_NAME);

    if (access("/sd/firmware.bin", F_OK) != -1) {
        firmware_upgrade("/sd/firmware.bin");
    }
}


void loop()
{
    bool wifi_available = strlen(WIFI_SSID) > 0;
    bool http_available = strlen(HTTP_UPDATE_URL) > 0 && (last_http_update == 0 ||
                            (start_time - last_http_update) >= HTTP_UPDATE_INTERVAL * 1000);

    if (!wifi_available) {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("WiFi disabled!\n");
    }
    else if (!http_available) {
        ESP_LOGI(__func__, "Polling sensors");
        Display.printf("Polling sensors!\n");
    }
    else {
        ESP_LOGI(__func__, "WiFi: Connecting to: '%s'...", WIFI_SSID);
        Display.printf("Connecting to\n %s...\n", WIFI_SSID);

        esp_log_level_set("wifi", ESP_LOG_WARN); // Wifi driver is *very* verbose
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        WiFi.setSleep(true);
    }

    // Read sensors while wifi connects
    readSensors();
    displaySensors();

    if (wifi_available && http_available) {
        while (WiFi.status() != WL_CONNECTED && millis() < (STATION_POLL_INTERVAL * 1000)) {
            WiFi.waitStatusBits(STA_HAS_IP_BIT, 500);
            Display.printf(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(__func__, "WiFi: Connected to: '%s' with IP %s", WIFI_SSID, WiFi.localIP().toString().c_str());
            Display.printf("Wifi connected!\nIP: %s\n", WiFi.localIP().toString().c_str());

            // Now do the HTTP Request
            httpRequest();
        }
        else {
            ESP_LOGW(__func__, "WiFi: Failed to connect to: '%s'", WIFI_SSID);
            Display.printf("\nFailed!");
        }
        WiFi.disconnect(true);
    }

    if (Display.isPresent()) {
        ls_delay(DISPLAY_TIMEOUT * 1000 - millis());
    }

    // Sleep
    hibernate();
}
