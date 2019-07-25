#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "WebServer.h"
#include "FwUpdater.h"
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
RTC_DATA_ATTR static bool use_sdcard = true;
static int STATION_POLL_INTERVAL = DEFAULT_STATION_POLL_INTERVAL;
static int HTTP_UPDATE_INTERVAL = DEFAULT_HTTP_UPDATE_INTERVAL;

ConfigProvider config;

extern const esp_app_desc_t esp_app_desc;

extern const uint8_t ulp_wind_bin_start[] asm("_binary_ulp_wind_bin_start");
extern const uint8_t ulp_wind_bin_end[]   asm("_binary_ulp_wind_bin_end");
// Maybe we should use a circular buffer so we can get average/mean/max/min?
extern uint32_t ulp_edge_count_max;
extern uint32_t ulp_loops_in_period;
extern uint32_t ulp_rtc_io;
extern uint32_t ulp_entry;

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

    ulp_rtc_io = rtc_gpio_desc[gpio_num].rtc_num; /* map from GPIO# to RTC_IO# */
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


float ulp_wind_read_kph()
{
    float rotations = (ulp_edge_count_max & UINT16_MAX) / 2;
    float rpm = rotations / (ulp_wind_sample_length_us / 1000 / 1000) * 60;
    float circ = (2 * 3.141592 * CFG_DBL("sensors.anemometer.radius")) / 100 / 1000;
    float kph = rpm * 60 * circ * CFG_DBL("sensors.anemometer.calibration");

    // Reset the counter
    ulp_edge_count_max = 0;

    return kph;
}


// True RTC millis count since the ESP32 was last reset
uint32_t rtc_millis()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((uint64_t)curTime.tv_sec * 1000000) + curTime.tv_usec) / 1000;
}


static bool debounceButton(int gpio, int level, int threshold = 50)
{
    int timeout = millis() + threshold;
    do {
        while (digitalRead(gpio) == level && millis() < timeout) {
            delay(10);
        }
        delay(10);
    } while (digitalRead(gpio) == level && millis() < timeout);

    return millis() >= timeout;
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
    // If we reach this point it means the updated app works.
    FwUpdater.markAppValid();

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


static bool firmware_upgrade_begin()
{
    Display.clear();
    Display.printf("Upgrading firmware...\n");

    if (!FwUpdater.begin()) {
        if (FwUpdater.getError() == FWU_ERR_OTA_PARTITION_IN_USE) {
            FwUpdater.rebootToFactory();
        }
        return false;
    }

    FwUpdater.onProgress([] (size_t a, size_t t) {
        static int count = 0;
        Display.printf(count++ % 22 ? "." : "\n");
    });

    return true;
}


static bool firmware_upgrade_end()
{
    if (FwUpdater.end()) {
        Display.printf("\n\nComplete!");
        return true;
    } else {
        Display.printf("\n\nFailed!\n\n%s", FwUpdater.getErrorStr());
        return false;
    }
}


static void firmware_upgrade_from_http(const char *url)
{
    ESP_LOGI(__func__, "Flashing firmware from '%s'...", url);

    if (firmware_upgrade_begin() && FwUpdater.writeFromHTTP(url) && firmware_upgrade_end()) {
        delay(10 * 1000);
        esp_restart();
    }
}


static void firmware_upgrade_from_file(const char *filePath)
{
    ESP_LOGI(__func__, "Flashing firmware from '%s'...", filePath);

    if (firmware_upgrade_begin() && FwUpdater.writeFromFile(filePath) && firmware_upgrade_end()) {
        String new_name = String(filePath) + "-installed.bin";
        SD.remove(new_name);
        SD.rename(filePath, new_name);
        delay(10 * 1000);
        esp_restart();
    }
}


static void startConfigurationServer(bool force_ap = false)
{
    uint32_t server_timeout = millis() + 15 * 60000;
    IPAddress server_ip = IPAddress(1, 1, 1, 1);
    char *ssid = CFG_STR("STATION.NAME");

    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);

    char *cfg_ssid = CFG_STR("WIFI.SSID"), *cfg_password = CFG_STR("WIFI.PASSWORD");
    if (!force_ap && strlen(cfg_ssid) > 0) {
        ESP_LOGI("AP", "Starting Configuration server on local wifi");
        Display.printf("Connecting...");

        WiFi.begin(cfg_ssid, cfg_password);
        WiFi.setSleep(true);
        WiFi.waitStatusBits(STA_HAS_IP_BIT, 20000);

        if (WiFi.status() == WL_CONNECTED) {
            ssid = cfg_ssid;
            server_ip = WiFi.localIP();
            ESP_LOGI("AP", "Wifi connected. SSID: %s  IP: %s", cfg_ssid, server_ip.toString().c_str());
        } else {
            ESP_LOGW("AP", "Unable to connect to '%s'", cfg_ssid);
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGI("AP", "Starting Configuration server on access point");

        if (WiFi.softAPConfig(server_ip, server_ip, IPAddress(255, 0, 0, 0)) && WiFi.softAP(ssid)) {
            ESP_LOGI("AP", "Access point started. SSID: %s  IP: %s", ssid, server_ip.toString().c_str());
        } else {
            ESP_LOGE("AP", "Unable to start the access point");
            return;
        }
    }

    WebServer server(80);
    ESP_LOGI("AP", "Server listening at http://%s:80", server_ip.toString().c_str());

    auto server_respond = [&](int code, String body) {
        server.send(code, "text/html",
                "<html><head><meta name=viewport content='width=device-width,initial-scale=0'>"
                "<style>label,input,a{font-size:2em;margin:0 5px}textarea{width:100%; height:75%}body"
                "{font-family:sans-serif;}</style></head><body><h1>" + String(CFG_STR("STATION.NAME"))
                + "</h1>" + body + "</body></html>");
    };
    server.on("/", [&]() {
        String body;
        if (server.method() == HTTP_POST) {
            if (config.loadJSON(server.arg("config").c_str())) {
                saveConfiguration(true);
                body += "<h2>Configuration saved!</h2>";
            } else {
                body += "<h2>Invalid JSON!</h2>";
            }
        }
        body += "<form method='post'><textarea name='config'>" + String(config.saveJSON()) + "</textarea>";
        body += "</textarea><p><input type='submit' value='Save'> <a href='/restart'>Restart</a></p></form><hr>";
        body += "<form action='/upload' method='post' enctype='multipart/form-data'><label>Update firmware:</label>";
        body += "<input type='file' name='file' style='max-width:50%'><input type='submit' value='Update'></form>";
        body.replace("\t", " ");

        server_respond(200, body);
    });
    server.on("/restart", [&] {
        server_respond(200, "Goodbye!");
        delay(1000);
        esp_restart();
    });
    server.on("/upload", HTTP_POST, [&] {
        server_respond(200, String("Status: <p>") + FwUpdater.getErrorStr() + "</p><a href='/restart'>Restart</a>");
    }, [&server]() {
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            firmware_upgrade_begin();
        }
        else if (upload.status == UPLOAD_FILE_WRITE) {
            if (!FwUpdater.write(upload.buf, upload.currentSize)) {
                ESP_LOGE("Upload", "Firmware update: Write error (%d bytes)", upload.currentSize);
            }
        }
        else if (upload.status == UPLOAD_FILE_END) {
            firmware_upgrade_end();
        }
    });
    server.begin();

    Display.clear();
    Display.printf("Configuration Server:\n\n");
    Display.printf(" SSID: %s\n\n", ssid);
    Display.printf(" http://%s\n", server_ip.toString().c_str());

    // Just in case the user is still holding the button
    debounceButton(ACTION_BUTTON_PIN, LOW, 10000);

    while (millis() < server_timeout || server.client().connected()) {
        server.handleClient();
        if (debounceButton(ACTION_BUTTON_PIN, LOW, 500)) {
            server.stop();
            startConfigurationServer(!force_ap);
        }
    }
}


static void checkActionButton()
{
    if (debounceButton(ACTION_BUTTON_PIN, LOW, 2500)) {
        startConfigurationServer();
        esp_restart();
    }
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
    PRINT_MEMORY_STATS(); const esp_partition_t *partition = esp_ota_get_running_partition();
    ESP_LOGI(__func__, "Partition: '%s', offset: 0x%x", partition->label, partition->address);

    if (wake_count == 0) {
        boot_time = rtc_millis();
        ulp_wind_start();
    }

    start_time = rtc_millis();
    wake_count++;

    // Power up our peripherals
    pinMode(PERIPH_POWER_PIN, OUTPUT);
    digitalWrite(PERIPH_POWER_PIN, PERIPH_POWER_PIN_LEVEL);
    delay(10); // Wait for peripherals to stabilize

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Display.begin();

    if (use_sdcard) {
        if ((use_sdcard = SD.begin())) {
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
    Display.printf("SD: %d | Up: %dm\n", use_sdcard ? 1 : 0, (start_time - boot_time) / 60000);

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
