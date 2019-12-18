#include "Arduino.h"
#include "WiFi.h"
#include "WebServer.h"
#include "FwUpdater.h"
#include "ConfigProvider.h"
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
    int64_t  uptime; // Uptime can work before NTP lock, timestamp cannot
    float    sensors_data[SENSORS_COUNT];
    uint32_t sensors_status;
} message_t;
const int MESSAGE_QUEUE_SIZE = 2048 / sizeof(message_t);

RTC_DATA_ATTR static int32_t wake_count = 0;
RTC_DATA_ATTR static int64_t first_boot_time = 0;
RTC_DATA_ATTR static int64_t next_http_update = 0;
RTC_DATA_ATTR static int64_t ntp_time_delta = 0;
RTC_DATA_ATTR static int64_t ntp_last_adjustment = 0;
RTC_DATA_ATTR static double  time_correction = 0;
RTC_DATA_ATTR static message_t message_queue[MESSAGE_QUEUE_SIZE];
RTC_DATA_ATTR static int16_t message_queue_pos = 0;
static bool is_interactive_wakeup = true;
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


int64_t rtc_millis()
{
    struct timeval curTime;
    gettimeofday(&curTime, NULL);
    return (((int64_t)curTime.tv_sec * 1000000) + curTime.tv_usec) / 1000;
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


static void loadConfiguration()
{
    config.loadNVS(CONFIG_USE_NVS);

    CFG_LOAD_STR("station.name", DEFAULT_STATION_NAME);
    CFG_LOAD_STR("station.group", DEFAULT_STATION_GROUP);
    CFG_LOAD_INT("station.poll_interval", DEFAULT_STATION_POLL_INTERVAL);
    CFG_LOAD_INT("station.display_timeout", DEFAULT_STATION_DISPLAY_TIMEOUT);
    CFG_LOAD_STR("station.display_content", DEFAULT_STATION_DISPLAY_CONTENT);
    CFG_LOAD_STR("wifi.ssid", DEFAULT_WIFI_SSID);
    CFG_LOAD_STR("wifi.password", DEFAULT_WIFI_PASSWORD);
    CFG_LOAD_INT("wifi.timeout", DEFAULT_WIFI_TIMEOUT);
    CFG_LOAD_STR("http.update.url", DEFAULT_HTTP_UPDATE_URL);
    CFG_LOAD_STR("http.update.type", DEFAULT_HTTP_UPDATE_TYPE);
    CFG_LOAD_STR("http.update.username", DEFAULT_HTTP_UPDATE_USERNAME);
    CFG_LOAD_STR("http.update.password", DEFAULT_HTTP_UPDATE_PASSWORD);
    CFG_LOAD_STR("http.update.database", DEFAULT_HTTP_UPDATE_DATABASE);
    CFG_LOAD_INT("http.update.interval", DEFAULT_HTTP_UPDATE_INTERVAL);
    CFG_LOAD_INT("http.timeout", DEFAULT_HTTP_TIMEOUT);
    CFG_LOAD_INT("http.ota.enabled", 1);
    CFG_LOAD_DBL("powersave.strategy", DEFAULT_POWER_SAVE_STRATEGY);
    CFG_LOAD_DBL("powersave.treshold", DEFAULT_POWER_SAVE_TRESHOLD);
    CFG_LOAD_DBL("sensors.adc.adc0_multiplier", DEFAULT_SENSORS_ADC_MULTIPLIER);
    CFG_LOAD_DBL("sensors.adc.adc1_multiplier", DEFAULT_SENSORS_ADC_MULTIPLIER);
    CFG_LOAD_DBL("sensors.adc.adc2_multiplier", DEFAULT_SENSORS_ADC_MULTIPLIER);
    CFG_LOAD_DBL("sensors.adc.adc3_multiplier", DEFAULT_SENSORS_ADC_MULTIPLIER);
    CFG_LOAD_DBL("sensors.anemometer.radius", DEFAULT_SENSORS_ANEMOMETER_RADIUS);
    CFG_LOAD_DBL("sensors.anemometer.calibration", DEFAULT_SENSORS_ANEMOMETER_CALIBRATION);
}


static void hibernate()
{
    // If we reach this point it means the updated app works.
    FwUpdater.markAppValid();

    // Cleanup
    Display.end();

    // See how much memory we never freed
    PRINT_MEMORY_STATS();

    // Sleep
    // To do: account for ESP32 boot time before millis timer is started (100+ ms)
    int sleep_time = (CFG_INT("STATION.POLL_INTERVAL") * 1000) - millis();

    if (sleep_time < 0) {
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
        sleep_time = 10 * 1000; // We could continue to loop() instead but I fear memory leaks
    }

    if (!is_interactive_wakeup && time_correction != 0.00) {
        int correction = round(time_correction * sleep_time);
        ESP_LOGI(__func__, "Time correction: Sleep: %dms Clock: %dms", -correction, correction);
        sleep_time += -correction;
        struct timeval time;
        gettimeofday(&time, NULL);
        time.tv_usec += correction * 1000;
        settimeofday(&time, NULL);
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


static void startConfigurationServer(bool force_ap = false)
{
    uint32_t server_timeout = millis() + 15 * 60000;
    IPAddress server_ip = IPAddress(1, 1, 1, 1);
    char *ssid = CFG_STR("station.name");

    char *cfg_ssid = CFG_STR("wifi.ssid"), *cfg_password = CFG_STR("wifi.password");
    if (!force_ap && strlen(cfg_ssid) > 0) {
        ESP_LOGI("AP", "Starting Configuration server on local wifi");
        Display.printf("Connecting...");

        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);
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

        WiFi.softAPdisconnect(true);
        WiFi.disconnect(true);

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
                config.saveNVS(CONFIG_USE_NVS, true);
                loadConfiguration();
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

        pollSensors();

        body += "<div><hr>Sensors:<table>";
        for (int i = 0; i < SENSORS_COUNT; i++) {
            body += "<tr>";
            body += "<td>" + String(SENSORS[i].key) + "</td>";
            body += "<td>" + String(SENSORS[i].desc) + "</td>";
            body += "<td>" + String(SENSORS[i].val) + "</td>";
            body += "<td>" + String(SENSORS[i].unit) + "</td>";
            body += "</tr>";
        }
        body += "</table></div>";

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
    Display.printf("SSID: %s\n\n", ssid);
    Display.printf("http://%s\n\n", server_ip.toString().c_str());
    Display.printf("Mode: %s\n", WiFi.getMode() == WIFI_MODE_STA ? "Local Client" : "Access point");
    Display.printf("(Press to toggle)");

    // Just in case the user is still holding the button
    debounceButton(ACTION_BUTTON_PIN, LOW, 10000);

    while (millis() < server_timeout || server.client().connected()) {
        server.handleClient();
        if (debounceButton(ACTION_BUTTON_PIN, LOW, 250)) {
            server.stop();
            startConfigurationServer(!force_ap);
            return;
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
    String url = CFG_STR("http.update.url");
    char content_type[40] = "text/plain";
    char buffer[2048] = "";
    int count = 0;

    if (strcasecmp(CFG_STR("http.update.type"), "InfluxDB") == 0)
    {
        strcpy(content_type, "application/binary");

        if (url.endsWith("/")) { url.remove(url.length() - 1); }
        url += "/write?db=" + String(CFG_STR("http.update.database")) + "&precision=ms";

        for (int i = 0; i < MESSAGE_QUEUE_SIZE; i++) {
            message_t *item = &message_queue[i];

            if (item->uptime == 0) continue; // Empty entry
            count++;

            sprintf(buffer + strlen(buffer),
                "%s_sensors,station=%s status=%u",
                CFG_STR("STATION.GROUP"),
                CFG_STR("STATION.NAME"),
                item->sensors_status
            );

            for (int j = 0; j < SENSORS_COUNT; j++) {
                if ((item->sensors_status & (1 << j)) == 0) {
                    sprintf(buffer + strlen(buffer), ",%s=%.4f", SENSORS[j].key, item->sensors_data[j]);
                }
            }

            sprintf(buffer + strlen(buffer), " %llu\n", first_boot_time + item->uptime);
        }

        sprintf(buffer + strlen(buffer),
            "%s_status,station=%s,version=%s,build=%s ntp_delta=%lld,data_points=%d,power_save=0,cycles=%d,uptime=%llu %llu",
            CFG_STR("STATION.GROUP"),
            CFG_STR("STATION.NAME"),
            PROJECT_VERSION,
            esp_app_desc.version,
            ntp_time_delta,
            count,
            wake_count,
            uptime(),
            boot_time()
        );
    }
    else
    {
        strcpy(content_type, "application/json");

        cJSON *json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "station", CFG_STR("STATION.NAME"));
        cJSON_AddStringToObject(json, "group", CFG_STR("STATION.GROUP"));
        cJSON_AddStringToObject(json, "version", PROJECT_VERSION);
        cJSON_AddStringToObject(json, "build", esp_app_desc.version);
        cJSON_AddNumberToObject(json, "uptime", uptime());
        cJSON_AddNumberToObject(json, "cycles", wake_count);
        cJSON_AddNumberToObject(json, "ntp_delta", ntp_time_delta);
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
                if ((item->sensors_status & (1 << i)) == 0) {
                    cJSON_AddNumberToObject(entry, SENSORS[i].key, F2D(item->sensors_data[i]));
                } else {
                    cJSON_AddNullToObject(entry, SENSORS[i].key); // Or maybe send nothing at all?
                }
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
    http_config.timeout_ms = CFG_INT("http.timeout") * 1000;
    if (strlen(CFG_STR("http.update.username")) > 0) {
        http_config.username   = CFG_STR("http.update.username");
        http_config.password   = CFG_STR("http.update.password");
        http_config.auth_type  = HTTP_AUTH_TYPE_BASIC;
    }

    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    esp_http_client_set_header(client, "Content-Type", content_type);
    esp_http_client_set_post_field(client, buffer, strlen(buffer));
    esp_err_t err = esp_http_client_perform(client);

    int httpCode = -1, length = -1;
    if (err == ESP_OK) {
        httpCode = esp_http_client_get_status_code(client);
        length = esp_http_client_read(client, buffer, sizeof(buffer) - 1);
        buffer[length > 0 ? length : 0] = '\0';
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

    int interval = POWER_SAVE_INTERVAL(CFG_INT("http.update.interval"), CFG_DBL("powersave.threshold"), getSensor("bat")->avg);
    if (interval < CFG_INT("http.update.interval")) {
        ESP_LOGW(__func__, "Power saving enabled, HTTP update interval increased to %ds", interval);
    }

    next_http_update = boot_time() + (interval * 1000);
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

        int64_t now_millis = ((int64_t)ntp_time.tv_sec * 1000000 + ntp_time.tv_usec) / 1000;
        ntp_time_delta = (now_millis - prev_millis);

        if (ntp_last_adjustment == 0) {
            first_boot_time += ntp_time_delta;
        } else {
            // The 0.5 is to reduce overshoot, it can be adjusted or removed if needed.
            time_correction += (double)ntp_time_delta / (now_millis - ntp_last_adjustment) * 0.5;
        }

        ntp_last_adjustment = now_millis;

        ESP_LOGI("NTP", "Received Time: %.24s, we were %lldms %s",
            ctime(&ntp_time.tv_sec), abs(ntp_time_delta), ntp_time_delta < 0 ? "ahead" : "behind");
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

    // This will determine how long the display is kept on and how we handle certain events
    is_interactive_wakeup = (wake_count == 1 || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0);

    // Power up our peripherals
    pinMode(PERIPH_POWER_PIN, OUTPUT);
    digitalWrite(PERIPH_POWER_PIN, PERIPH_POWER_PIN_LEVEL);
    delay(10); // Wait for peripherals to stabilize

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Display.begin();

    loadConfiguration();

    ESP_LOGI(__func__, "Station name: '%s', group: '%s'", CFG_STR("STATION.NAME"), CFG_STR("STATION.GROUP"));
    Display.printf("# %s #\n", CFG_STR("STATION.NAME"));
    Display.printf("# Up: %lld minutes #\n", uptime() / 60000);

    // Our button can always interrupt light and deep sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)ACTION_BUTTON_PIN, LOW);
    rtc_gpio_pulldown_dis((gpio_num_t)ACTION_BUTTON_PIN);
    rtc_gpio_pullup_en((gpio_num_t)ACTION_BUTTON_PIN);

    // Check if we detect a long press
    checkActionButton();
}


void loop()
{
    const char *wifi_ssid = CFG_STR("wifi.ssid"), *wifi_password = CFG_STR("wifi.password");
    const int  wifi_timeout_ms = CFG_INT("wifi.timeout") * 1000;

    const bool wifi_available = strlen(wifi_ssid) > 0;
    const bool http_available = strlen(CFG_STR("http.update.url")) > 0 && rtc_millis() >= (next_http_update - 5000);

    if (!wifi_available) {
        ESP_LOGI(__func__, "WiFi: No configuration");
        Display.printf("\nWiFi disabled!");
    }
    else if (!http_available) {
        ESP_LOGI(__func__, "Polling sensors");
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
            // First check if messages are waiting for us
            httpPullData();
            // Then push all our sensors data over HTTP
            httpPushData();
        }
        else {
            ESP_LOGW(__func__, "WiFi: Failed to connect to: '%s'", wifi_ssid);
            Display.printf("\nFailed!");
        }
        WiFi.disconnect(true);
        delay(200); // This resolves a crash
    }


    // Keep the screen on for a while
    int display_timeout = (is_interactive_wakeup ? CFG_INT("STATION.DISPLAY_TIMEOUT") : 1) * 1000 - millis();

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
