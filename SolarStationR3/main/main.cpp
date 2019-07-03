#include "Arduino.h"
#include "ConfigProvider.h"
#include "DHT.h"
#include "SD.h"

#include "helpers/display.h"
#include "helpers/config.h"
#include "helpers/time.h"
#include "config.h"

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
    //esp_deep_sleep(sleep_time * 1000);
    esp_sleep_enable_timer_wakeup(sleep_time * 1000);
    //esp_light_sleep_start();
    esp_deep_sleep_start();
}


void setup()
{
    printf("\n################### WEATHER STATION (Ver: %s) ###################\n\n", PROJECT_VER);
    ESP_LOGI("Uptime", "Uptime: %d seconds (Cycles: %d)", (rtc_millis() - boot_time) / 1000, wake_count);
    PRINT_MEMORY_STATS();

    if (boot_time == 0) {
        boot_time = rtc_millis();
    }

    start_time = rtc_millis();
    wake_count++;

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

    PRINT_MEMORY_STATS();
}


void loop()
{
    Display.printf("Hello world!\n");

    float t, h, p;
    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
        Display.printf("DHT: %.2f %.2f\n", t, h);
    }

    delay(DISPLAY_TIMEOUT * 1000);

    ESP_LOGI("loop", "Looperino");
    hibernate();
}
