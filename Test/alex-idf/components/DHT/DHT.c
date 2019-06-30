static const char *MODULE = "DHT";

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "FreeRTOS/FreeRTOS.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "DHT.h"

static uint32_t pulse_time(uint8_t pin, bool level)
{
    uint32_t start = esp_timer_get_time();
    uint32_t timeout = start + 1000;

    while (gpio_get_level(pin) == level && esp_timer_get_time() <= timeout);

    return esp_timer_get_time() - start;
}


bool dht_read(uint8_t type, uint8_t pin, float *tempC, float *humidity)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);

    if (type == DHT22) {
        usleep(2 * 1000); // PULL LOW 0.8-20ms
    } else {
        usleep(20 * 1000); // PULL LOW 20ms
    }

    // Pull high and wait 40 microseconds
    gpio_set_level(pin, 1);
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    usleep(40);

    uint8_t data[5] = {0};

    // Timing-sensitive section.
    portDISABLE_INTERRUPTS();

    // Next the sensor will pull low (~80us), then high (~80us)
    // If either takes too long we bail
    if (
        pulse_time(pin, 0) > 1000 ||
        pulse_time(pin, 1) > 1000
    ) {
        portENABLE_INTERRUPTS();
        ESP_LOGW(MODULE, "Pulse timeout!");
        return false;
    }

    int l, h;
    for (int i = 0; i < 40; i++) {
        l = pulse_time(pin, 0);
        h = pulse_time(pin, 1);

        data[i/8] <<= 1;
        data[i/8] |= (h > 40);     // specs: 28us -> 0, 70us -> 1
        //data[i/8] |= (h > l); // This also works
    }

    portENABLE_INTERRUPTS();

    // Check we read 40 bits and that the checksum matches. From Adafruit_DHT
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        float t, h;

        if (type == DHT22) {
            t = ((uint32_t)(data[2] & 0x7F)) << 8 | data[3];
            t *= 0.1;
            if (data[2] & 0x80) {
                t *= -1;
            }
            h = ((uint32_t)data[0]) << 8 | data[1];
            h *= 0.1;
        }
        else { // Probably DHT11
            t = data[2];
            if (data[3] & 0x80) {
                t = -1 - t;
            }
            t += (data[3] & 0x0f) * 0.1;
            h = data[0] + data[1] * 0.1;
        }

        *tempC = t;
        *humidity = h;

        return true;
    }
    else {
        ESP_LOGW(MODULE, "checksum failure!");
        return false;
    }
}
