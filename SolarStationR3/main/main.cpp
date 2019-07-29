#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "FwUpdater.h"
#include "ConfigProvider.h"
#include "SD.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "sys/time.h"
#include "sys/socket.h"
#include "netinet/in.h"
#include "netdb.h"

#include "config.h"
#include "macros.h"
#include "display.h"
#include "sensors.h"

typedef struct {
    int64_t uptime; // Uptime can work before NTP lock, timestamp cannot
    float sensors_data[SENSORS_COUNT];
    uint32_t sensors_status;
} message_t;
const int MESSAGE_QUEUE_SIZE = 2048 / sizeof(message_t);

RTC_DATA_ATTR static int32_t wake_count = 0;
RTC_DATA_ATTR static int64_t first_boot_time = 0;
RTC_DATA_ATTR static int64_t next_http_update = 0;
RTC_DATA_ATTR static message_t message_queue[MESSAGE_QUEUE_SIZE];
RTC_DATA_ATTR static int16_t message_queue_pos = 0;
RTC_DATA_ATTR static bool first_boot_time_adjusted = false;
RTC_DATA_ATTR static bool use_sdcard = true;
static int32_t STATION_POLL_INTERVAL = DEFAULT_STATION_POLL_INTERVAL;
static int32_t HTTP_UPDATE_INTERVAL = DEFAULT_HTTP_UPDATE_INTERVAL;

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
int64_t rtc_millis()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((uint64_t)curTime.tv_sec * 1000000) + curTime.tv_usec) / 1000;
}


int64_t uptime()
{
    return (rtc_millis() - first_boot_time);
}


int64_t boot_time()
{
    return (rtc_millis() - millis());
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
    CFG_LOAD_STR("http.update.type", DEFAULT_HTTP_UPDATE_TYPE);
    CFG_LOAD_STR("http.update.username", DEFAULT_HTTP_UPDATE_USERNAME);
    CFG_LOAD_STR("http.update.password", DEFAULT_HTTP_UPDATE_PASSWORD);
    CFG_LOAD_STR("http.update.database", DEFAULT_HTTP_UPDATE_DATABASE);
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


static void httpPullData()
{
    // Here we will do an HTTP request to see if messages were left for us.
    // Config and firmware updates for example, or reboot/startconfigserver
}


static void httpPushData()
{
    String url = CFG_STR("HTTP.UPDATE.URL");
    char content_type[40] = "text/plain";
    char buffer[2048] = "";
    int count = 0;

    if (strcasecmp(CFG_STR("HTTP.UPDATE.TYPE"), "InfluxDB") == 0)
    {
        strcpy(content_type, "application/binary");

        if (url.endsWith("/")) { url.remove(url.length() - 1); }
        url += "/write?db=" + String(CFG_STR("HTTP.UPDATE.DATABASE")) + "&precision=ms";

        for (int i = 0; i < MESSAGE_QUEUE_SIZE; i++) {
            message_t *item = &message_queue[i];

            if (item->uptime == 0) continue; // Empty entry
            count++;

            sprintf(buffer + strlen(buffer),
                "%s,station=%s,version=%s,build=%s uptime=%llu",
                "weather", // Need to add a setting for that
                CFG_STR("STATION.NAME"),
                PROJECT_VERSION,
                esp_app_desc.version,
                item->uptime
            );

            for (int j = 0; j < SENSORS_COUNT; j++) {
                sprintf(buffer + strlen(buffer), ",%s=%.4f", SENSORS[j].key, item->sensors_data[j]);
            }

            sprintf(buffer + strlen(buffer), " %llu\n", first_boot_time + item->uptime);
        }
    }
    else
    {
        strcpy(content_type, "application/json");

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "station", CFG_STR("STATION.NAME"));
        cJSON_AddStringToObject(json, "version", PROJECT_VERSION);
        cJSON_AddStringToObject(json, "build", esp_app_desc.version);
        cJSON_AddNumberToObject(json, "uptime", uptime());
        cJSON_AddNumberToObject(json, "cycles", wake_count);
        cJSON *data = cJSON_AddArrayToObject(json, "data");

        for (int i = 0; i < MESSAGE_QUEUE_SIZE; i++) {
            message_t *item = &message_queue[i];

            if (item->uptime == 0) continue; // Empty entry
            count++;

            cJSON *entry = cJSON_CreateObject();
            cJSON_AddNumberToObject(entry, "time", first_boot_time + item->uptime);
            cJSON_AddNumberToObject(entry, "offset", uptime() - item->uptime);
            cJSON_AddNumberToObject(entry, "status", item->sensors_status);
            for (int i = 0; i < SENSORS_COUNT; i++) {
                cJSON_AddNumberToObject(entry, SENSORS[i].key, F2D(item->sensors_data[i]));
            }
            cJSON_AddItemToArray(data, entry);
        }

        cJSON_PrintPreallocated(json, buffer, sizeof(buffer), false);
        cJSON_free(json);
    }

    ESP_LOGI(__func__, "HTTP: Sending %d data frame(s) to '%s'...", count, url.c_str());
    ESP_LOGI(__func__, "HTTP: Body: '%s'", buffer);
    Display.printf("\nHTTP POST...");

    esp_http_client_config_t http_config = {};

    http_config.url        = url.c_str();
    http_config.method     = HTTP_METHOD_POST;
    http_config.timeout_ms = CFG_INT("HTTP.TIMEOUT") * 1000;
    if (strlen(CFG_STR("HTTP.UPDATE.USERNAME")) > 0) {
        http_config.username   = CFG_STR("HTTP.UPDATE.USERNAME");
        http_config.password   = CFG_STR("HTTP.UPDATE.PASSWORD");
        http_config.auth_type  = HTTP_AUTH_TYPE_BASIC;
    }

    // Auth Workaround waiting for https://github.com/espressif/esp-idf/issues/3843
    if (http_config.username && http_config.password) {
        url.replace("://", String("://") + http_config.username + ":" + http_config.password + "@");
        http_config.url = url.c_str();
    }

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, buffer, strlen(buffer));
    esp_err_t err = esp_http_client_perform(client);

    int httpCode = -1, length = -1;
    if (err == ESP_OK) {
        httpCode = esp_http_client_get_status_code(client);
        length = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        buffer[length > 0 ? length : 0] = NULL;
    }

    if (httpCode == 200 || httpCode == 204) {
        ESP_LOGI(__func__, "HTTP: Received code: %d  Body: '%s'", httpCode, buffer);
        Display.printf("OK (%d)", httpCode);
        memset(message_queue, 0, sizeof(message_queue)); // Request successful, clear items from queue!
    }
    else if (httpCode > 0) {
        ESP_LOGW(__func__, "HTTP: Received code: %d  Body: '%s'", httpCode, buffer);
        Display.printf("Failed (%d)", httpCode);
    }
    else {
        ESP_LOGE(__func__, "HTTP: Request failed: %s", esp_err_to_name(err));
        Display.printf("Failed (%s)", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);

    next_http_update = boot_time() + (HTTP_UPDATE_INTERVAL * 1000);
}


// We don't use the built-in sntp because we need to be synchronous and also detect drift
void ntpTimeUpdate(const char *host = NTP_SERVER_1)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct hostent *server = gethostbyname(host);
    struct sockaddr_in serv_addr = {};
    struct timeval timeout = { 2, 0 };
    struct timeval ntp_time = {0, 0};

    if (server == NULL) {
        ESP_LOGE("NTP", "Unable to resolve NTP server");
        return;
    }

    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(123);

    uint32_t ntp_packet[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ((uint8_t*)ntp_packet)[0] = 0x1b; // li, vn, mode.

    int64_t prev_millis = rtc_millis();

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    connect(sockfd, (sockaddr *)&serv_addr, sizeof(serv_addr));
    send(sockfd, &ntp_packet, sizeof(ntp_packet), 0);

    if (recv(sockfd, &ntp_packet, sizeof(ntp_packet), 0) < 0) {
        ESP_LOGE("NTP", "Error receiving NTP packet");
    }
    else {
        prev_millis += (rtc_millis() - prev_millis) / 2; // Approx transport time
        ntp_time.tv_sec = ntohl(ntp_packet[10]) - 2208988800UL; // DIFF_SEC_1900_1970
        ntp_time.tv_usec = ((int64_t)ntohl(ntp_packet[11]) * 1000000) >> 32;
        settimeofday(&ntp_time, NULL);
        const int64_t time_delta = rtc_millis() - prev_millis;

        if (!first_boot_time_adjusted) {
            first_boot_time += time_delta;
            first_boot_time_adjusted = true;
        }

        ESP_LOGI("NTP", "Received Time: %.24s, we are %lldms %s",
            ctime(&ntp_time.tv_sec), abs(time_delta), time_delta < 0 ? "ahead" : "behind");
    }
}


void setup()
{
    printf("\n################### WEATHER STATION (Version: %s) ###################\n\n", PROJECT_VERSION);
    ESP_LOGI(__func__, "Build: %s (%s %s)", esp_app_desc.version, esp_app_desc.date, esp_app_desc.time);
    ESP_LOGI(__func__, "Uptime: %llu seconds (Cycles: %lu)", uptime() / 1000, wake_count);
    PRINT_MEMORY_STATS(); const esp_partition_t *partition = esp_ota_get_running_partition();
    ESP_LOGI(__func__, "Partition: '%s', offset: 0x%x", partition->label, partition->address);

    if (wake_count++ == 0) {
        first_boot_time = rtc_millis();
        ulp_wind_start();
    }

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
    Display.printf("SD: %d | Up: %dm\n", use_sdcard ? 1 : 0, uptime() / 60000);

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
    const bool http_available = strlen(CFG_STR("HTTP.UPDATE.URL")) > 0 && rtc_millis() >= (next_http_update - 5000);

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


    // Add sensors data to message (HTTP) queue
    message_t *item = &message_queue[message_queue_pos];
    item->uptime = uptime();
    for (int i = 0; i < SENSORS_COUNT; i++) {
        item->sensors_data[i] = SENSORS[i].val;
        item->sensors_status |= ((SENSORS[i].status ? 1 : 0) << i);
    }
    message_queue_pos = (message_queue_pos + 1) % MESSAGE_QUEUE_SIZE;


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
            // We don't have to do it every time, but since our RTC drifts 250ms per minute...
            ntpTimeUpdate();
            // Now push all our sensors data over HTTP
            httpPushData();
            // Now check if messages are waiting for us (should be before push?)
            httpPullData();
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
