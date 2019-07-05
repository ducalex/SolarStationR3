#include "Adafruit_BMP085.h"
#include "Adafruit_ADS1015.h"
#include "DHT.h"
#include "avgr.h"
#include "../config.h"

static RTC_DATA_ATTR AVGr m_battery_Volt(60);
static RTC_DATA_ATTR AVGr m_solar_Volt(60);
static RTC_DATA_ATTR AVGr m_lightsensor1_RAW(60);
static RTC_DATA_ATTR AVGr m_lightsensor2_RAW(60);
static RTC_DATA_ATTR AVGr m_temperature1_C(12);
static RTC_DATA_ATTR AVGr m_temperature2_C(12);
static RTC_DATA_ATTR AVGr m_humidity1_Pct(12);
static RTC_DATA_ATTR AVGr m_humidity2_Pct(12);
static RTC_DATA_ATTR AVGr m_pressure1_kPa(12);
static RTC_DATA_ATTR AVGr m_pressure2_kPa(12);

static void readSensors()
{
    Adafruit_BMP085 bmp;
    float t = 0, h = 0, p = 0;

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        m_temperature1_C.add(t);
        m_humidity1_Pct.add(h);
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    }

    if (bmp.begin()) {
        t = bmp.readTemperature();
        p = (float)bmp.readPressure() / 1000;
        m_temperature2_C.add(t);
        m_pressure1_kPa.add(p);
        ESP_LOGI(__func__, "BMP: %.2f %d", t, p);
    }
}


static char *serializeSensors()
{
    char buffer[512] = {0};
    sprintf(buffer,
        "&bat=%.4f"
        "&sol=%.4f"
        "&l1=%.4f"
        "&l2=%.4f"
        "&t1=%.4f"
        "&t2=%.4f"
        "&h1=%.4f"
        "&h2=%.4f"
        "&p1=%.4f"
        "&p2=%.4f",
        m_battery_Volt.getAvg(),
        m_solar_Volt.getAvg(),
        m_lightsensor1_RAW.getAvg(),
        m_lightsensor2_RAW.getAvg(),
        m_temperature1_C.getAvg(),
        m_temperature2_C.getAvg(),
        m_humidity1_Pct.getAvg(),
        m_humidity2_Pct.getAvg(),
        m_pressure1_kPa.getAvg(),
        m_pressure2_kPa.getAvg()
    );

    return strdup(buffer);
}
