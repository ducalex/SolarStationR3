#include <driver/rtc_io.h>
#include <esp32/ulp.h>
#include <esp_log.h>

extern const uint8_t ulp_wind_bin_start[] asm("_binary_ulp_wind_bin_start");
extern const uint8_t ulp_wind_bin_end[]   asm("_binary_ulp_wind_bin_end");
// Maybe we should use a circular buffer so we can get average/mean/max/min?
extern uint32_t ulp_edge_count_max;
extern uint32_t ulp_loops_in_period;
extern uint32_t ulp_rtc_io;
extern uint32_t ulp_entry;

const uint32_t ulp_wind_sample_length_us = 10 * 1000 * 1000;
const uint32_t ulp_wind_period_us = 500;

void ulp_wind_start(gpio_num_t gpio_num)
{
    esp_err_t err = ulp_load_binary(0, ulp_wind_bin_start,
            (ulp_wind_bin_end - ulp_wind_bin_start) / sizeof(uint32_t));

    if (err != ESP_OK) {
        ESP_LOGE("ULP", "Failed to load the binary! (%s)", esp_err_to_name(err));
        return;
    }

    assert(rtc_io_desc[gpio_num].reg && "GPIO used for pulse counting must be an RTC IO");

    ulp_rtc_io = rtc_io_desc[gpio_num].rtc_num; /* map from GPIO# to RTC_IO# */
    ulp_loops_in_period = ulp_wind_sample_length_us / ulp_wind_period_us;

    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(gpio_num);
    rtc_gpio_pullup_en(gpio_num);
    rtc_gpio_hold_en(gpio_num);

    ulp_set_wakeup_period(0, ulp_wind_period_us);

    err = ulp_run(&ulp_entry - RTC_SLOW_MEM);

    if (err == ESP_OK) {
        ESP_LOGI("ULP", "Wind program started!");
    } else {
        ESP_LOGE("ULP", "Failed to start the program! (%s)", esp_err_to_name(err));
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
