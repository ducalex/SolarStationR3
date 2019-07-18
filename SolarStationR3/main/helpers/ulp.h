#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "esp_log.h"
//#include "ulp_wind.h"

extern const uint8_t ulp_wind_bin_start[] asm("_binary_ulp_wind_bin_start");
extern const uint8_t ulp_wind_bin_end[]   asm("_binary_ulp_wind_bin_end");

extern uint32_t ulp_edge_count_max;
extern uint32_t ulp_entry;
extern uint32_t ulp_io_number;
extern uint32_t ulp_loops_in_period;

const uint32_t sample_length_us = 10 * 1000 * 1000;
const uint32_t period_us = 500;
const uint32_t loops_in_period = sample_length_us / period_us;

void ulp_wind_start()
{
    esp_err_t err = ulp_load_binary(0, ulp_wind_bin_start,
            (ulp_wind_bin_end - ulp_wind_bin_start) / sizeof(uint32_t));

    if (err != ESP_OK) {
        ESP_LOGE("ULP", "Failed to load the ULP binary! (%s)", esp_err_to_name(err));
        return;
    }

    gpio_num_t gpio_num = (gpio_num_t)ANEMOMETER_PIN;

    /* GPIO used for pulse counting. */
    assert(rtc_gpio_desc[gpio_num].reg && "GPIO used for pulse counting must be an RTC IO");

    ulp_io_number = rtc_gpio_desc[gpio_num].rtc_num; /* map from GPIO# to RTC_IO# */
    ulp_loops_in_period = loops_in_period;

    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(gpio_num);
    rtc_gpio_pullup_en(gpio_num);
    rtc_gpio_hold_en(gpio_num);

    ulp_set_wakeup_period(0, period_us);

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
    int rpm = (int)(rotations / (sample_length_us / 1000 / 1000) * 60);

    // Reset the counter
    ulp_edge_count_max = 0;

    // I don't know how stable the ULP is over thousands of cycles
    // ulp_wind_start();

    return rpm;
}
