#include <esp_attr.h>
#include <esp_log.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "helpers/avgr.h"
#include "display.h"
#include "config.h"

#include "DHT.h"


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
    float t, h;
    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    } else {
        ESP_LOGI(__func__, "DHT FAILED");
    }
}


void sensors_print()
{
    display_printf("Sensors values: \n");
    display_printf(" Battery: %.2fV\n", m_batteryVoltage.avg);
    display_printf(" Temp: %.2fC\n", m_exteriorTempC.avg);
    display_printf(" Humidity: %.2f%%\n", m_exteriorHumidity.avg);
    display_printf(" Pressure: %.2fkPa\n", m_exteriorPressure.avg);
}


char* sensors_serialize()
{
    return strdup("void");
}
