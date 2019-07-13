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
#include "helpers/time.h"
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


static void ls_delay(uint32_t ms)
{
    ESP_LOGI(__func__, "Light sleeping for %dms", ms);
    delay(50); // Time for the uart hardware buffer to empty
    esp_sleep_enable_timer_wakeup((ms - 50) * 1000);
    esp_light_sleep_start();
}


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
    int interval_ms = e_poll_interval * 1000;
    int sleep_time = interval_ms - millis();

    if (sleep_time < 1000) {
        sleep_time = interval_ms > 1000 ? interval_ms : 1000;
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
    }

    ESP_LOGI(__func__, "Deep sleeping for %dms", sleep_time);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_BUTTON_PIN, LOW);
    //esp_sleep_enable_touchpad_wakeup();
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

        ESP_LOGI(__func__, "HTTP(%d): Sending: '%s'", count, payload);

        int httpCode = http.POST(payload);
        ESP_LOGI(__func__, "HTTP(%d): Return code: %d", count, httpCode);

        if (httpCode > 0) {
            ESP_LOGI(__func__, "HTTP(%d): Received: '%s'", count, http.getString().c_str());
            Display.printf("%d\n", httpCode);
        } else {
            Display.printf("error: %s\n", http.errorToString(httpCode).c_str());
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
    bool http_available = strlen(HTTP_UPDATE_URL) > 0 && start_time >= next_http_update;

    if (!wifi_available) {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("WiFi disabled!\n");
    }
    else if (!http_available) {
        ESP_LOGI(__func__, "Polling sensors");
        // Display.printf("Polling sensors!\n");
    }
    else {
        ESP_LOGI(__func__, "WiFi: Connecting to: '%s'...", WIFI_SSID);
        Display.printf("Connecting to\n %s...\n", WIFI_SSID);

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
        while (WiFi.status() != WL_CONNECTED && millis() < (WIFI_TIMEOUT * 1000)) {
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
        delay(200); // This resolves a crash
    }


    if (Display.isPresent()) {
        ls_delay(STATION_DISPLAY_TIMEOUT * 1000 - millis());
    }

    // Sleep
    hibernate();
}
