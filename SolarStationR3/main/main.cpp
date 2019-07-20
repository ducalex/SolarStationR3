#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "WebServer.h"
#include "ConfigProvider.h"
#include "SD.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "sys/time.h"

#include "config.h"
#include "macros.h"
#include "display.h"
#include "sensors.h"

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
static int STATION_POLL_INTERVAL = DEFAULT_STATION_POLL_INTERVAL;
static int HTTP_UPDATE_INTERVAL = DEFAULT_HTTP_UPDATE_INTERVAL;

ConfigProvider config;

extern const esp_app_desc_t esp_app_desc;

extern const uint8_t ulp_wind_bin_start[] asm("_binary_ulp_wind_bin_start");
extern const uint8_t ulp_wind_bin_end[]   asm("_binary_ulp_wind_bin_end");

extern uint32_t ulp_edge_count_max;
extern uint32_t ulp_entry;
extern uint32_t ulp_io_number;
extern uint32_t ulp_loops_in_period;

const uint32_t ulp_wind_sample_length_us = 10 * 1000 * 1000;
const uint32_t ulp_wind_period_us = 500;

void ulp_wind_start()
{
    esp_err_t err = ulp_load_binary(0, ulp_wind_bin_start,
            (ulp_wind_bin_end - ulp_wind_bin_start) / sizeof(uint32_t));

    if (err != ESP_OK) {
        ESP_LOGE("ULP", "Failed to load the ULP binary! (%s)", esp_err_to_name(err));
        return;
    }

    gpio_num_t gpio_num = (gpio_num_t)ANEMOMETER_PIN;
    assert(rtc_gpio_desc[gpio_num].reg && "GPIO used for pulse counting must be an RTC IO");

    ulp_io_number = rtc_gpio_desc[gpio_num].rtc_num; /* map from GPIO# to RTC_IO# */
    ulp_loops_in_period = ulp_wind_sample_length_us / ulp_wind_period_us;

    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(gpio_num);
    rtc_gpio_pullup_en(gpio_num);
    rtc_gpio_hold_en(gpio_num);

    ulp_set_wakeup_period(0, ulp_wind_period_us);

    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);

    if (err == ESP_OK) {
        ESP_LOGI("ULP", "ULP wind program started!");
    } else {
        ESP_LOGE("ULP", "Failed to start the ULP binary! (%s)", esp_err_to_name(err));
    }
}


int ulp_wind_read()
{
    float rotations = (ulp_edge_count_max & UINT16_MAX) / 2;
    int rpm = (int)(rotations / (ulp_wind_sample_length_us / 1000 / 1000) * 60);

    // Reset the counter
    ulp_edge_count_max = 0;

    return rpm;
}


// True RTC millis count since the ESP32 was last reset
uint32_t rtc_millis()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((uint64_t)curTime.tv_sec * 1000000) + curTime.tv_usec) / 1000;
}


static void saveConfiguration(bool update_only)
{
    if (SD.cardType() != CARD_NONE) {
        config.saveFile(CONFIG_USE_FILE, update_only);
    }
    config.saveNVS(CONFIG_USE_NVS, update_only);
}


static void loadConfiguration()
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
    CFG_LOAD_DBL("sensors.anemometer.radius", DEFAULT_SENSORS_ANEMOMETER_RADIUS);
    CFG_LOAD_DBL("sensors.anemometer.calibration", DEFAULT_SENSORS_ANEMOMETER_CALIBRATION);

    saveConfiguration(true);
}


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
    SD.end();

    // See how much memory we never freed
    PRINT_MEMORY_STATS();

    // Sleep
    int sleep_time = (STATION_POLL_INTERVAL * 1000) - millis();

    if (sleep_time < 0) {
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
        sleep_time = 10 * 1000; // We could continue to loop() instead but I fear memory leaks
    }

    ESP_LOGI(__func__, "Deep sleeping for %dms", sleep_time);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    esp_deep_sleep_start();
}


static void startAccessPoint()
{
    const char *AP_SSID = CFG_STR("STATION.NAME");
    const IPAddress AP_IP = IPAddress(1, 1, 1, 1);

    ESP_LOGI("AP", "Starting Configuration access point");

    if (WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 0, 0, 0)) && WiFi.softAP(AP_SSID)) {
        ESP_LOGI("AP", "SSID: %s   IP: %s", AP_SSID, AP_IP.toString().c_str());
    } else {
        ESP_LOGE("AP", "Unable to start the access point");
        return;
    }

    SD.begin();
    WebServer server(80);
    server.on("/", [&server]() {
        if (server.hasArg("restart")) {
            server.send(200, "text/plain", "Goodbye!");
            delay(1000);
            WiFi.softAPdisconnect(true);
            esp_restart();
        }

        String page = "<html><head>";
        page += "<meta name=viewport content='width=device-width, initial-scale=0'>";
        page += "<style>input{font-size:2em;}textarea{width:100%; height:80%;}</style>";
        page += "</head><body>";
        if (server.method() == HTTP_POST) {
            if (config.loadJSON(server.arg("config").c_str())) {
                saveConfiguration(true);
                page += "<h2>Configuration saved!</h2>";
            } else {
                page += "<h2>Invalid JSON!</h2>";
            }
        } else {
            page += "<h2>Hello!</h2>";
        }
        page += "<form method='post'><div>";
        page += "<textarea name='config'>";
        page += config.saveJSON();
        page += "</textarea></div>";
        page += "<input type='submit' value='Save'> <input type='submit' name='restart' value='Restart'>";
        page += "</form></body></html>";

        page.replace("\t", " ");
        server.send(200, "text/html", page);
    });
    server.begin();

    Display.clear();
    Display.printf("Configuration AP:\n\n");
    Display.printf(" SSID: %s\n\n", AP_SSID);
    Display.printf(" http://%s\n", AP_IP.toString().c_str());

    disableCore1WDT(); // Server library isn't friendly to our dog

    while (true) {
        server.handleClient();
    }
}


static void checkActionButton()
{
    int start = millis();
    do {
        while (digitalRead(ACTION_BUTTON_PIN) == LOW) {
            if ((millis() - start) > 2500) {
                startAccessPoint();
                esp_restart();
            }
            delay(50);
        }
        delay(20);
    } while (digitalRead(ACTION_BUTTON_PIN) == LOW);
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
    char *url = CFG_STR("HTTP.UPDATE.URL"), *username = CFG_STR("HTTP.UPDATE.USERNAME");
    char *password = CFG_STR("HTTP.UPDATE.PASSWORD");

    ESP_LOGI(__func__, "HTTP: POST request(s) to '%s'...", url);

    uint count = 0;
    char payload[512];

    for (int i = 0; i < HTTP_QUEUE_MAX_ITEMS; i++) {
        http_item_t *item = &http_queue[i];

        if (item->timestamp == 0) { // Empty entry
            continue;
        }

        count++;

        HTTPClient http; // I don't know if it is reusable

        http.begin(url);
        http.setTimeout(CFG_INT("HTTP.TIMEOUT") * 1000);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        if (strlen(username) > 0) {
            http.setAuthorization(username, password);
        }

        char *sensors_data = serializeSensors(item->sensors_data);

        sprintf(payload,
            "station=%s&app=%s+%s&ps_http=%.2f&ps_poll=%.2f&uptime=%d" "&offset=%d&cycles=%d&status=%d&%s",
            CFG_STR("STATION.NAME"),      // Current station name
            PROJECT_VERSION,              // Build version information
            esp_app_desc.version,         // Build version information
            (float)STATION_POLL_INTERVAL / CFG_INT("STATION.POLL_INTERVAL"), // Current power saving mode
            (float)HTTP_UPDATE_INTERVAL / CFG_INT("HTTP.UPDATE.INTERVAL"),  // Current power saving mode
            start_time - boot_time,       // Current uptime (ms)
            item->timestamp - start_time, // Age of entry relative to now (ms)
            item->wake_count,             // Wake count at time of capture
            item->sensors_status,         // Each bit represents a sensor. 0 = no error
            sensors_data                  // Sensors data at time of capture
        );
        free(sensors_data);

        Display.printf("\nHTTP POST...");

        ESP_LOGI(__func__, "HTTP(%d): Sending body: '%s'", count, payload);
        const int httpCode = http.POST(payload);
        const char *httpBody = http.getString().c_str();

        if (httpCode >= 400) {
            ESP_LOGW(__func__, "HTTP(%d): Received code: %d  Body: '%s'", count, httpCode, httpBody);
            Display.printf("Failed (%d)", httpCode);
        }
        else if (httpCode > 0) {
            ESP_LOGI(__func__, "HTTP(%d): Received code: %d  Body: '%s'", count, httpCode, httpBody);
            Display.printf("OK (%d)", httpCode);
            memset(item, 0, sizeof(http_item_t)); // Request successful, clear item from queue!
        }
        else {
            Display.printf("Failed (%s)", http.errorToString(httpCode).c_str());
        }

        http.end();
    }

    next_http_update = start_time + (HTTP_UPDATE_INTERVAL * 1000);
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
        ulp_wind_start();
        // Ideally we'd try to mount it on every boot but if the station has no SD Card
        // then it takes about 600ms to timeout, .6*60*24 = 15m/day.
        // Or about 12mAh per day wasted just waiting.
        if (SD.begin()) {
            ESP_LOGI(__func__, "SD Card successfully mounted.");
            if (SD.exists("firmware.bin")) {
                firmware_upgrade_from_file("firmware.bin");
            }
        } else {
            ESP_LOGW(__func__, "Failed to mount SD Card.");
        }
    }

    loadConfiguration();

    ESP_LOGI(__func__, "Station name: %s", CFG_STR("STATION.NAME"));
    Display.printf("# %s #\n", CFG_STR("STATION.NAME"));
    Display.printf("SD: %s | Up: %dm\n", "?", (start_time - boot_time) / 60000);

    // Our button can always interrupt light and deep sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ACTION_BUTTON_PIN, LOW);
    rtc_gpio_pulldown_dis((gpio_num_t)ACTION_BUTTON_PIN);
    rtc_gpio_pullup_en((gpio_num_t)ACTION_BUTTON_PIN);

    // Check if we detect a long press
    checkActionButton();
}


void loop()
{
    const char *wifi_ssid = CFG_STR("WIFI.SSID"), *wifi_password = CFG_STR("WIFI.PASSWORD");
    const int  wifi_timeout_ms = CFG_INT("WIFI.TIMEOUT") * 1000;

    const bool wifi_available = strlen(wifi_ssid) > 0;
    const bool http_available = strlen(CFG_STR("HTTP.UPDATE.URL")) > 0 && start_time >= next_http_update;

    if (!wifi_available) {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("\nWiFi disabled!");
    }
    else if (!http_available) {
        ESP_LOGI(__func__, "Polling sensors");
        // Display.printf("\nPolling sensors!");
    }
    else {
        ESP_LOGI(__func__, "WiFi: Connecting to: '%s'...", wifi_ssid);
        Display.printf("\nConnecting to\n %s...", wifi_ssid);

        esp_log_level_set("wifi", ESP_LOG_WARN); // Wifi driver is *very* verbose
        WiFi.begin(wifi_ssid, wifi_password);
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
    float vbat = getSensor("bat")->avg; // We use avg so it doesn't bounce around too much
    STATION_POLL_INTERVAL = POWER_SAVE_INTERVAL(
        CFG_INT("STATION.POLL_INTERVAL"), CFG_DBL("POWER.POLL_LOW_VBAT_TRESHOLD"), vbat);
    HTTP_UPDATE_INTERVAL = POWER_SAVE_INTERVAL(
        CFG_INT("HTTP.UPDATE.INTERVAL"), CFG_DBL("POWER.HTTP_LOW_VBAT_TRESHOLD"), vbat);

    ESP_LOGI(__func__, "Effective intervals: poll: %d (%d), http: %d (%d), vbat: %.2f",
        STATION_POLL_INTERVAL, CFG_INT("STATION.POLL_INTERVAL"),
        HTTP_UPDATE_INTERVAL, CFG_INT("HTTP.UPDATE.INTERVAL"), vbat);


    // Now do the http request!
    if (wifi_available && http_available) {
        Display.printf("\n");
        while (WiFi.status() != WL_CONNECTED && millis() < wifi_timeout_ms) {
            WiFi.waitStatusBits(STA_HAS_IP_BIT, 500);
            Display.printf(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            String local_ip = WiFi.localIP().toString();
            ESP_LOGI(__func__, "WiFi: Connected to: '%s' with IP %s", wifi_ssid, local_ip.c_str());
            Display.printf("Connected!\nIP: %s", local_ip.c_str());

            // Now do the HTTP Request
            httpRequest();
        }
        else {
            ESP_LOGW(__func__, "WiFi: Failed to connect to: '%s'", wifi_ssid);
            Display.printf("\nFailed!");
        }
        WiFi.disconnect(true);
        delay(200); // This resolves a crash
    }


    // Keep the screen on for a while
    int display_timeout = CFG_INT("STATION.DISPLAY_TIMEOUT") * 1000 - millis();

    if (Display.isPresent() && display_timeout > 50) {
        ESP_LOGI(__func__, "Display will timeout in %dms (entering light-sleep)", display_timeout);
        delay(50); // Time for the uart hardware buffer to empty
        esp_sleep_enable_timer_wakeup((display_timeout - 50) * 1000);
        esp_light_sleep_start();
        checkActionButton(); // Mostly to debounce
    }


    // Deep-sleep
    hibernate();
}
