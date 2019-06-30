#include <esp_attr.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "helpers/avgr.h"
#include "display.h"
#include "config.h"

#include "BMP180.h"
#include "DHT.h"

static const char *TAG = "sensors";

// Global sensor values
RTC_DATA_ATTR static AVGr m_batteryVoltage   = new_AVGr(64);
RTC_DATA_ATTR static AVGr m_lightsensorRAW   = new_AVGr(64);

RTC_DATA_ATTR static AVGr m_exteriorTempC    = new_AVGr(12);
RTC_DATA_ATTR static AVGr m_exteriorPressure = new_AVGr(12);
RTC_DATA_ATTR static AVGr m_exteriorHumidity = new_AVGr(12);
RTC_DATA_ATTR static AVGr m_interiorTempC    = new_AVGr(12);
RTC_DATA_ATTR static AVGr m_interiorHumidity = new_AVGr(12);


void sensors_init()
{

}


void sensors_deinit()
{

}


void sensors_sleep()
{

}


void sensors_poll()
{
    float t, h, p;
    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        ESP_LOGI(TAG, "DHT: %.2f %.2f", t, h);
        avgr_add(&m_exteriorTempC, t);
        avgr_add(&m_exteriorHumidity, h);
    } else {
        ESP_LOGE(TAG, "DHT FAILED");
    }

    if (BMP180_read(I2C_NUM_0, BMP180_I2CADDR, &t, &p)) {
        ESP_LOGI(TAG, "BMP: %.2f %.2f", t, p);
    } else {
        ESP_LOGE(TAG, "BMP FAILED");
    }
}


void sensors_print()
{
    display_printf("Sensors values: \n");
    display_printf(" Battery: %.2fV\n", m_batteryVoltage.val);
    display_printf(" Temp: %.2fC\n", m_exteriorTempC.val);
    display_printf(" Humidity: %.2f%%\n", m_exteriorHumidity.val);
    display_printf(" Pressure: %.2fkPa\n", m_exteriorPressure.val);
}


char* sensors_serialize()
{
    return strdup("void");
}
