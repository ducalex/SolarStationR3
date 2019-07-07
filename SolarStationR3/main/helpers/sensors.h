#include "Adafruit_ADS1015.h"
#include "BMP180.h"
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
static RTC_DATA_ATTR AVGr m_windSpeed_kmh(2);
static RTC_DATA_ATTR AVGr m_windDirection_deg(2);
static RTC_DATA_ATTR AVGr m_rain_RAW(2);

static void readSensors()
{
    Adafruit_ADS1115 ads;
    float t = 0, h = 0, p = 0;

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        m_temperature1_C.add(t);
        m_humidity1_Pct.add(h);
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    } else {
        // add 0?
        ESP_LOGE(__func__, "DHT sensor not responding");
    }

    if (BMP180_read(BMP180_I2C_ADDR, &t, &p)) {
        m_temperature2_C.add(t);
        m_pressure1_kPa.add(p);
        ESP_LOGI(__func__, "BMP: %.2f %.2f", t, p);
    } else {
        // add 0?
        ESP_LOGE(__func__, "BMP180 sensor not responding");
    }

    ads.begin();
    ads.setGain(GAIN_TWOTHIRDS);
    m_battery_Volt.add(ads.readADC_SingleEnded(3));
    m_solar_Volt.add(ads.readADC_SingleEnded(0));
    ads.setGain(GAIN_ONE);
    m_lightsensor1_RAW.add(ads.readADC_SingleEnded(1));
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



static void displaySensors()
{
    Display.printf("Volt: %.2f %.2f\n", m_battery_Volt.getVal(), m_solar_Volt.getVal());
    Display.printf("Light: %d %d\n", m_lightsensor1_RAW.getVal(), m_lightsensor2_RAW.getVal());
    Display.printf("Temp: %.2f %.2f\n", m_temperature1_C.getVal(), m_temperature2_C.getVal());
    Display.printf("Humidity: %.0f %.0f\n", m_humidity1_Pct.getVal(), m_humidity2_Pct.getVal());
    Display.printf("Pressure: %.2f %.2f\n", m_pressure1_kPa.getVal(), m_pressure2_kPa.getVal());
}
