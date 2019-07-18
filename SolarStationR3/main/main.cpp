#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "ConfigProvider.h"
#include "SD.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "config.h"
// Most of these should have their own .cpp file, but I'm lazy :(
#include "helpers/userconfig.h"
#include "helpers/time.h"
#include "helpers/ulp.h"
#include "helpers/display.h"
#include "helpers/sensors.h"

#define HTTP_QUEUE_MAX_ITEMS 40
typedef struct {
    uint32_t timestamp;
    uint32_t wake_count;
    float sensors_data[SENSORS_COUNT];
    uint32_t sensors_status;
} http_item_t;

RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static uint32_t boot_time = 0;
RTC_DATA_ATTR static uint32_t start_time = 0;
RTC_DATA_ATTR static uint32_t next_http_update = 0;
RTC_DATA_ATTR static http_item_t http_queue[HTTP_QUEUE_MAX_ITEMS];
RTC_DATA_ATTR static uint16_t http_queue_pos = 0;

static uint32_t e_poll_interval; // Effective interval. Normally = STATION_POLL_INTERVAL
static uint32_t e_http_interval; // Effective interval. Normally = HTTP_UPDATE_INTERVAL

extern const esp_app_desc_t esp_app_desc;

#define POWER_SAVE_INTERVAL(in, th, vb) (((float)th <= vb || vb < 2) ? in : (uint)ceil(((th-vb) * 100.00) * in))
#define PRINT_MEMORY_STATS() { \
  multi_heap_info_t info; \
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT); \
  ESP_LOGI("Memory", "Memory: Used: %d KB   Free: %d KB", \
        info.total_allocated_bytes / 1024, info.total_free_bytes / 1024); }


static void hibernate()
{
    // If we reach this point it means the app works, let's disable rollback
    if (esp_ota_check_rollback_is_possible()) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            ESP_LOGI(__func__, "Rollback: App has now been marked as OK!");
            esp_ota_erase_last_boot_app_partition();
        }
    }

    // Cleanup
    Display.end();

    // See how much memory we never freed
    PRINT_MEMORY_STATS();

    // Sleep
    int sleep_time = (e_poll_interval * 1000) - millis();

    if (sleep_time < 0) {
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
        sleep_time = 10 * 1000; // We could continue to loop() instead but I fear memory leaks
    }

    ESP_LOGI(__func__, "Deep sleeping for %dms", sleep_time);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_BUTTON_PIN, LOW); // Todo: check current usage vs touch
    esp_deep_sleep_start();
}


static bool firmware_upgrade_from_stream(Stream &stream)
{
    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *current = esp_ota_get_running_partition();
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

    if (target == NULL) {
        if (factory != NULL && current != factory) {
            ESP_LOGW(__func__, "No free OTA partition. Rebooting to factory image to flash from there.");
            esp_ota_set_boot_partition(factory);
            esp_restart();
        }
        ESP_LOGE(__func__, "No free OTA partition. Cannot upgrade");
        return false;
    }

    ESP_LOGI(__func__, "Flashing firmware to 0x%x...", target->address);

    Display.clear();
    Display.printf("Upgrading firmware...\n");

    uint8_t *buffer = (uint8_t*)malloc(16 * 1024);
    esp_ota_handle_t ota;
    esp_err_t err;
    size_t size, count = 0;

    err = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota);
    if (err != ESP_OK) goto finish;

    while ((size = stream.readBytes(buffer, 16 * 1024)) > 0) {
        err = esp_ota_write(ota, buffer, size);
        if (err != ESP_OK) goto finish;
        Display.printf(count++ % 22 ? "." : "\n");
    }

    err = esp_ota_end(ota);
    if (err != ESP_OK) goto finish;

    err = esp_ota_set_boot_partition(target);
    if (err != ESP_OK) goto finish;

  finish:
    free(buffer);
    if (err == ESP_OK) {
        Display.printf("\n\nComplete!");
        ESP_LOGI(__func__, "Firmware successfully flashed!");
    } else {
        ESP_LOGE(__func__, "Firmware upgrade failed: %s", esp_err_to_name(err));
        Display.printf("\n\nFailed!\n\n%s", esp_err_to_name(err));
    }
    return (err == ESP_OK);
}


static void firmware_upgrade_from_http(const char *url)
{
    ESP_LOGI(__func__, "Flashing firmware from '%s'...", url);

    HTTPClient http;

    if (firmware_upgrade_from_stream(http.getStream())) {
        delay(10 * 1000);
        esp_restart();
    }
    vTaskDelete(NULL);
}


static void firmware_upgrade_from_file(const char *filePath)
{
    ESP_LOGI(__func__, "Flashing firmware from '%s'...", filePath);

    fs::File file = SD.open(filePath);
    if (!file) {
        ESP_LOGE(__func__, "Unable to open file!");
        return;
    }

    if (firmware_upgrade_from_stream(file)) {
        file.close();
        String new_name = String(filePath) + "-installed.bin";
        SD.remove(new_name);
        SD.rename(filePath, new_name);
        delay(10 * 1000);
        esp_restart();
    }
    vTaskDelete(NULL);
}


static void httpRequest()
{
    ESP_LOGI(__func__, "HTTP: POST request(s) to '%s'...", HTTP_UPDATE_URL);

    short count = 0;
    char payload[512];

    for (int i = 0; i < HTTP_QUEUE_MAX_ITEMS; i++) {
        http_item_t *item = &http_queue[i];

        if (item->timestamp == 0) { // Empty entry
            continue;
        }

        count++;

        HTTPClient http; // I don't know if it is reusable

        http.begin(HTTP_UPDATE_URL);
        http.setTimeout(HTTP_UPDATE_TIMEOUT * 1000);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        if (strlen(HTTP_UPDATE_USERNAME) > 0) {
            http.setAuthorization(HTTP_UPDATE_USERNAME, HTTP_UPDATE_PASSWORD);
        }

        char *sensors_data = serializeSensors(item->sensors_data);

        sprintf(payload, "station=%s&app=%s+%s&ps_http=%.2f&ps_poll=%.2f&uptime=%d" "&offset=%d&cycles=%d&status=%d&%s",
            STATION_NAME,                 // Current station name
            PROJECT_VERSION,              // Build version information
            esp_app_desc.version,         // Build version information
            (float)e_poll_interval / STATION_POLL_INTERVAL, // Current power saving mode
            (float)e_http_interval / HTTP_UPDATE_INTERVAL,  // Current power saving mode
            start_time - boot_time,       // Current uptime (ms)
            item->timestamp - start_time, // Age of entry relative to now (ms)
            item->wake_count,             // Wake count at time of capture
            item->sensors_status,         // Each bit represents a sensor. 0 = no error
            sensors_data                  // Sensors data at time of capture
        );
        free(sensors_data);

        Display.printf("\nHTTP POST...");

        ESP_LOGI(__func__, "HTTP(%d): Sending body: '%s'", count, payload);
        int httpCode = http.POST(payload);

        if (httpCode >= 400) {
            ESP_LOGW(__func__, "HTTP(%d): Received code: %d  Body: '%s'", count, httpCode, http.getString().c_str());
            Display.printf("Failed (%d)", httpCode);
        }
        else if (httpCode > 0) {
            ESP_LOGI(__func__, "HTTP(%d): Received code: %d  Body: '%s'", count, httpCode, http.getString().c_str());
            Display.printf("OK (%d)", httpCode);
        }
        else {
            Display.printf("Failed (%s)", http.errorToString(httpCode).c_str());
        }

        http.end();
        memset(item, 0, sizeof(http_item_t)); // Or do it only on success?
    }

    next_http_update = start_time + (e_http_interval * 1000);
}


void setup()
{
    printf("\n################### WEATHER STATION (Version: %s) ###################\n\n", PROJECT_VERSION);
    ESP_LOGI(__func__, "Build: %s (%s %s)", esp_app_desc.version, esp_app_desc.date, esp_app_desc.time);
    ESP_LOGI(__func__, "Uptime: %d seconds (Cycles: %d)", (rtc_millis() - boot_time) / 1000, wake_count);
    PRINT_MEMORY_STATS();

    if (boot_time == 0) {
        boot_time = rtc_millis();
    }

    start_time = rtc_millis();
    wake_count++;

    // Power up our peripherals
    pinMode(PERIPH_POWER_PIN, OUTPUT);
    digitalWrite(PERIPH_POWER_PIN, PERIPH_POWER_PIN_LEVEL);
    delay(10); // Wait for peripherals to stabilize

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Display.begin();

    if (wake_count == 1) {
        if (SD.begin()) {
            ESP_LOGI(__func__, "SD Card successfully mounted.");
            if (SD.exists("firmware.bin")) {
                firmware_upgrade_from_file("firmware.bin");
            }
        } else {
            ESP_LOGW(__func__, "Failed to mount SD Card.");
        }

        loadConfiguration();
        ulp_wind_start();

        SD.end();
    }

    ESP_LOGI(__func__, "Station name: %s", STATION_NAME);
    Display.printf("# %s #\n", STATION_NAME);
    Display.printf("SD: %s | Up: %dm\n", "?", (start_time - boot_time) / 60000);
}


void loop()
{
    bool wifi_available = strlen(WIFI_SSID) > 0;
    bool http_available = strlen(HTTP_UPDATE_URL) > 0 && start_time >= next_http_update;

    if (!wifi_available) {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("\nWiFi disabled!");
    }
    else if (!http_available) {
        ESP_LOGI(__func__, "Polling sensors");
        // Display.printf("\nPolling sensors!");
    }
    else {
        ESP_LOGI(__func__, "WiFi: Connecting to: '%s'...", WIFI_SSID);
        Display.printf("\nConnecting to\n %s...", WIFI_SSID);

        esp_log_level_set("wifi", ESP_LOG_WARN); // Wifi driver is *very* verbose
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        WiFi.setSleep(true);
    }


    // Poll sensors while wifi connects
    pollSensors();
    displaySensors();


    // Add sensors data to HTTP queue
    http_item_t *item = &http_queue[http_queue_pos];
    item->timestamp = start_time;
    item->wake_count = wake_count;
    packSensors(item->sensors_data, &item->sensors_status);
    http_queue_pos = (http_queue_pos + 1) % HTTP_QUEUE_MAX_ITEMS;


    // Adjust our intervals based on the battery we've just read
    e_poll_interval = POWER_SAVE_INTERVAL(STATION_POLL_INTERVAL, POWER_POLL_LOW_VBAT_TRESHOLD, m_battery_Volt.avg);
    e_http_interval = POWER_SAVE_INTERVAL(HTTP_UPDATE_INTERVAL, POWER_HTTP_LOW_VBAT_TRESHOLD, m_battery_Volt.avg);

    ESP_LOGI(__func__, "Effective intervals: poll: %d (%d), http: %d (%d), vbat: %.2f",
        e_poll_interval, STATION_POLL_INTERVAL, e_http_interval, HTTP_UPDATE_INTERVAL, m_battery_Volt.avg);


    // Now do the http request!
    if (wifi_available && http_available) {
        Display.printf("\n");
        while (WiFi.status() != WL_CONNECTED && millis() < (WIFI_TIMEOUT * 1000)) {
            WiFi.waitStatusBits(STA_HAS_IP_BIT, 500);
            Display.printf(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            ESP_LOGI(__func__, "WiFi: Connected to: '%s' with IP %s", WIFI_SSID, WiFi.localIP().toString().c_str());
            Display.printf("Connected!\nIP: %s", WiFi.localIP().toString().c_str());

            // Now do the HTTP Request
            httpRequest();
        }
        else {
            ESP_LOGW(__func__, "WiFi: Failed to connect to: '%s'", WIFI_SSID);
            Display.printf("\nFailed!");
        }
        WiFi.disconnect(true);
        delay(200); // This resolves a crash
    }


    // Keep the screen on for a while
    int display_timeout = STATION_DISPLAY_TIMEOUT * 1000 - millis();

    if (Display.isPresent() && display_timeout > 50) {
        ESP_LOGI(__func__, "Display will timeout in %dms (entering light-sleep)", display_timeout);
        delay(50); // Time for the uart hardware buffer to empty
        esp_sleep_enable_timer_wakeup((display_timeout - 50) * 1000);
        esp_light_sleep_start();
    }


    // Deep-sleep
    hibernate();
}
