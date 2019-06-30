//extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_adc_cal.h"
#include "esp_partition.h"
#include "esp_event_loop.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_vfs_fat.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_http_client.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "nvs_flash.h"
#include "rom/crc.h"
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "cfgparser.h"
#include "display.h"
#include "sensors.h"

#include "helpers/time.h"
#include "helpers/i2c.h"

#include "config.h"
//}

static char *STATION_NAME;         // 32
static char *WIFI_SSID;            // 32
static char *WIFI_PASSWORD;        // 64
static char *HTTP_UPDATE_URL;      // 128
static char *HTTP_UPDATE_USERNAME; // 64
static char *HTTP_UPDATE_PASSWORD; // 64
static int   HTTP_UPDATE_TIMEOUT;  // Seconds
static int   POLL_INTERVAL;        // Seconds
static int   POLL_METHOD;          //

static sdmmc_card_t* sdcard;
static bool sdcard_mounted;
static char tmpBuffer[512];
static uint32_t start_time;
static FILE *logFile;

static EventGroupHandle_t event_group;

RTC_DATA_ATTR static uint32_t wake_count = 0;
RTC_DATA_ATTR static uint32_t boot_time = 0;

#define WIFI_CONNECTED_BIT BIT0
#define _check ESP_ERROR_CHECK
#define _debug ESP_ERROR_CHECK_WITHOUT_ABORT
#define file_exists(f) (access(f, F_OK) != -1)


void debug_memory_stats()
{
  multi_heap_info_t info;
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
  ESP_LOGI("Memory", "Used: %d KB   Free: %d KB",
        info.total_allocated_bytes / 1024, info.total_free_bytes / 1024);
}


void cleanup_and_restart()
{
    if (logFile) {
        fclose(logFile);
    }

    if (sdcard_mounted) {
        esp_vfs_fat_sdmmc_unmount();
    }

    esp_restart();
}


void fatal_error(char *error)
{
    // Display error on screen or blinky LED
    // print error to serial
    // Stop all timers and go to deep sleep
}


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI("wifi_event", "Got IP: %s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            ESP_LOGI("wifi_event", "Connected to '%s'", event->event_info.connected.ssid);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            if (event->event_info.disconnected.reason < 200) { // wifi_err_reason_t
                ESP_LOGI("wifi_event", "Disconnected from '%s' (%d)",
                    event->event_info.disconnected.ssid, event->event_info.disconnected.reason);
            } else {
                ESP_LOGW("wifi_event", "Connecting to '%s' failed (%d)",
                    event->event_info.disconnected.ssid, event->event_info.disconnected.reason);
            }
            xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
            break;
        default:
            ESP_LOGD("wifi_event", "Event %d", event->event_id);
            break;
    }
    return ESP_OK;
}


static void wifi_scan()
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    _check( esp_wifi_scan_start(&scan_config, true) );

    uint16_t ap_num;
    wifi_ap_record_t ap_records[32];

    _check( esp_wifi_scan_get_ap_records(&ap_num, ap_records) );

    printf("Found %d access points:\n", ap_num);

    for (int i = 0; i < ap_num; i++) {
        printf("%32s | %7d | %4d | %d\n",
            (char *)ap_records[i].ssid, ap_records[i].primary, ap_records[i].rssi,
            ap_records[i].authmode);
    }
}


static void wifi_init()
{
    ESP_LOGI(__func__, "Connecting to: '%s'", WIFI_SSID);

    // Init TCP/IP Stack
    tcpip_adapter_init();

    // Init Wifi
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, 31);
    if (WIFI_PASSWORD != NULL) {
        strncpy((char*)wifi_config.sta.password, WIFI_PASSWORD, 63);
    }

    _debug( esp_wifi_init(&wifi_init_cfg) );
    _debug( esp_wifi_set_mode(WIFI_MODE_STA) );
    _debug( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    _debug( esp_wifi_start() );
    _debug( esp_wifi_connect() );
}


static bool wifi_is_connected(int ticks)
{
    return (xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, false, false, ticks) & WIFI_CONNECTED_BIT);
}


static void wifi_deinit()
{
    esp_wifi_stop();
    esp_wifi_deinit();
}


static void sdcard_init()
{
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = SDMMC_FREQ_PROBING;

    sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
    slot_config.gpio_miso = (gpio_num_t)SD_PIN_NUM_MISO;
    slot_config.gpio_mosi = (gpio_num_t)SD_PIN_NUM_MOSI;
    slot_config.gpio_sck  = (gpio_num_t)SD_PIN_NUM_CLK;
    slot_config.gpio_cs   = (gpio_num_t)SD_PIN_NUM_CS;

    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 8
    };

    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &sdcard);
    sdcard_mounted = (ret == ESP_OK);

    if (ret != ESP_OK) {
        ESP_LOGE(__func__, "Failed to mound SD Card: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(__func__, "Successfully mounted SD Card");
    }
}


static void sdcard_deinit()
{
    esp_vfs_fat_sdmmc_unmount();
}


static void firmware_upgrade(const char *file)
{
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
    const esp_partition_t *target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    const esp_partition_t *current = esp_ota_get_running_partition();

    if (current->address != factory->address) {
        // As we have a single OTA, we can only flash from factory. We reboot to it.
        esp_ota_set_boot_partition(factory);
        cleanup_and_restart();
    }

    // Danger zone
    assert(target != NULL);

    ESP_LOGI(__func__, "Flashing file %s to 0x%x", file, target->address);

    FILE *fp = fopen(file, "r+");
    if (fp == NULL) {
        ESP_LOGE(__func__, "Unable to open file!");
        return;
    }

    void *buffer = malloc(16 * 1024);

    esp_ota_handle_t ota;
    size_t size;

    _check( esp_ota_begin(target, OTA_SIZE_UNKNOWN, &ota) );

    while ((size = fread(buffer, 1, 16 * 1024, fp)) > 0) {
        _check( esp_ota_write(ota, buffer, size) );
    }

    _check( esp_ota_end(ota) );
    _check( esp_ota_set_boot_partition(target) );

    free(buffer);
    fclose(fp);

    // Rename file after successful install
    strcpy(tmpBuffer, file);
    tmpBuffer[strlen(tmpBuffer)-4] = 0;
    strcat(tmpBuffer, "_installed.bin");

    rename(file, tmpBuffer);

    ESP_LOGI(__func__, "Firmware successfully flashed!");

    cleanup_and_restart();
}


static void http_request(void *data)
{
    display_printf("Performing HTTP request...\n");

    esp_http_client_config_t config = {
        .url = HTTP_UPDATE_URL,
        .method = HTTP_METHOD_POST,
        .username = HTTP_UPDATE_USERNAME,
        .password = HTTP_UPDATE_PASSWORD,
        .timeout_ms = HTTP_UPDATE_TIMEOUT * 1000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, data, strlen(data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        int length = esp_http_client_get_content_length(client);

        // The reply will be a json string that might contain:
        // - The current server time to adjust our RTC
        // - (Optional) An update url to fetch an OTA update
        // - (Optional) Configuration update

        ESP_LOGI(__func__, "HTTP status = %d, content_length = %d", code, length);
        display_printf("OK\nstatus = %d, length = %d\n", code, length);
    }
    else {
        ESP_LOGE(__func__, "HTTP request failed: %s", esp_err_to_name(err));
        display_printf("FAIL\nError: %s\n", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}


//extern "C"
void app_main()
{
    printf("\n################### WEATHER STATION (Ver: "PROJECT_VER") ###################\n\n");
    ESP_LOGI("Uptime", "%d seconds (Cycles: %d)", (rtc_millis() - boot_time) / 1000, wake_count);
    debug_memory_stats();

    if (boot_time == 0) {
        boot_time = rtc_millis();
    }

    start_time = rtc_millis();
    wake_count++;

    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,
        //.min_freq_mhz = 40,
        .light_sleep_enable = true
    };
    esp_pm_configure(&pm_config);

    // Reduce verbosity of some logs
    esp_log_level_set("wifi", ESP_LOG_WARN);

    event_group = xEventGroupCreate();
    esp_event_loop_init(event_handler, NULL);

    // Init NVS for wifi calibration and our settings
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        _debug( nvs_flash_erase() );
        _debug( nvs_flash_init() );
    }

    i2c_fast_init(I2C_NUM_0, I2C_SDA_PIN, I2C_SCL_PIN, 200 * 1000);

    display_init();
    sdcard_init();

    if (file_exists("/sdcard/firmware.bin")) {
        firmware_upgrade("/sdcard/firmware.bin");
    }

    if (config_load_file("/sdcard/config.json")) {
        config_save_nvs("config");
    } else {
        config_load_nvs("config");
    }

    STATION_NAME         = config_get_string("station.name", "Station");
    WIFI_SSID            = config_get_string("wifi.ssid", NULL);
    WIFI_PASSWORD        = config_get_string("wifi.password", NULL);
    HTTP_UPDATE_URL      = config_get_string("http.update_url", NULL);
    HTTP_UPDATE_USERNAME = config_get_string("http.update_username", NULL);
    HTTP_UPDATE_PASSWORD = config_get_string("http.update_password", NULL);
    HTTP_UPDATE_TIMEOUT  = config_get_int("http.update_timeout", 30);
    POLL_INTERVAL        = config_get_int("poll.interval", 30);
    POLL_METHOD          = config_get_int("poll.method", 0);

    bool use_wifi = (WIFI_SSID != NULL) && (HTTP_UPDATE_URL != NULL);

    // Connect to wifi
    if (use_wifi) {
        //ESP_LOGI(__func__, "Connecting to wifi '%s'...", WIFI_SSID);
        display_printf("Connecting to '%s'...\n", WIFI_SSID);
        wifi_init();
    }
    else {
        ESP_LOGW(__func__, "Can't connect to wifi: Missing configuration!");
        display_printf("No wifi configuration!\n");
    }


    // Poll the sensors while wifi connects
    sensors_init();
    sensors_poll();


    // Wait for wifi for up to 15 seconds
    if (use_wifi && wifi_is_connected(15 * 100)) {
        display_clear();
        display_printf("Wifi connected!\n");

        // Do the http request
        void *sensors_data = sensors_serialize();
        http_request(sensors_data);
        free(sensors_data);
    }


    // Show sensors to the user before going to sleep
    display_clear();
    sensors_print();


    // Before we clean up, log how much memory we used
    debug_memory_stats();


    // Shutdown devices cleanly
    sensors_deinit();
    sdcard_deinit();

    if (use_wifi) {
        wifi_deinit();
    }


    // Let the user read the screen before going to sleep
    delay(5 * 1000);
    display_deinit();

    // One last time
    debug_memory_stats();


    // Sleep
    int interval_ms = POLL_INTERVAL * 1000;
    int sleep_time = interval_ms - (rtc_millis() - start_time);

    if (sleep_time < 100) {
        sleep_time = MAX(100, interval_ms);
        ESP_LOGW(__func__, "Bogus sleep time, did we spend too much time processing?");
    }

    ESP_LOGI(__func__, "Sleeping for %dms", sleep_time);
    esp_deep_sleep(sleep_time * 1000);
}
