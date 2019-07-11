#include "Adafruit_ADS1015.h"
#include "BMP180.h"
#include "DHT.h"
#include "avgr.h"
#include "../config.h"

static RTC_DATA_ATTR AVGr m_battery_Volt(60);
static RTC_DATA_ATTR AVGr m_solar_Volt(60);
static RTC_DATA_ATTR AVGr m_lightsensor1_RAW(60);
static RTC_DATA_ATTR AVGr m_lightsensor2_RAW(60);
static RTC_DATA_ATTR AVGr m_temperature1_C(10);
static RTC_DATA_ATTR AVGr m_temperature2_C(10);
static RTC_DATA_ATTR AVGr m_humidity1_Pct(10);
static RTC_DATA_ATTR AVGr m_humidity2_Pct(10);
static RTC_DATA_ATTR AVGr m_pressure1_kPa(10);
static RTC_DATA_ATTR AVGr m_pressure2_kPa(10);
static RTC_DATA_ATTR AVGr m_windSpeed_kmh(2);
static RTC_DATA_ATTR AVGr m_windDirection_deg(2);
static RTC_DATA_ATTR AVGr m_rain_RAW(2);

typedef struct {
    float battery_Volt;
    float solar_Volt;
    float lightsensor1_RAW;
    float lightsensor2_RAW;
    float temperature1_C;
    float temperature2_C;
    float humidity1_Pct;
    float humidity2_Pct;
    float pressure1_kPa;
    float pressure2_kPa;
} sensors_data_t;


static void pollSensors()
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
    ads.setGain(GAIN_ONE); // real range is vdd + 0.3

    for (int i = 0; i < 10; i++) {
        m_battery_Volt.add(0.000125 * (int16_t)ads.readADC_SingleEnded(0) * POWER_VBAT_MULTIPLIER);
        m_solar_Volt.add(0.000125 * (int16_t)ads.readADC_SingleEnded(1));
        m_lightsensor1_RAW.add(0.000125 * (int16_t)ads.readADC_SingleEnded(2));
        delay(5);
    }

    delay(15); // ADC can still be in the bus, crappy thingy

    ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f",
        m_battery_Volt.getVal(), m_solar_Volt.getVal(), m_lightsensor1_RAW.getVal());
}


static sensors_data_t readSensors(bool getCurrentValue = false)
{
    sensors_data_t out_frame;

    if (getCurrentValue) {
        out_frame.battery_Volt = m_battery_Volt.getVal();
        out_frame.solar_Volt = m_solar_Volt.getVal();
        out_frame.lightsensor1_RAW = m_lightsensor1_RAW.getVal();
        out_frame.lightsensor2_RAW = m_lightsensor2_RAW.getVal();
        out_frame.temperature1_C = m_temperature1_C.getVal();
        out_frame.temperature2_C = m_temperature2_C.getVal();
        out_frame.humidity1_Pct = m_humidity1_Pct.getVal();
        out_frame.humidity2_Pct = m_humidity2_Pct.getVal();
        out_frame.pressure1_kPa = m_pressure1_kPa.getVal();
        out_frame.pressure2_kPa = m_pressure2_kPa.getVal();
    } else {
        out_frame.battery_Volt = m_battery_Volt.getAvg();
        out_frame.solar_Volt = m_solar_Volt.getAvg();
        out_frame.lightsensor1_RAW = m_lightsensor1_RAW.getAvg();
        out_frame.lightsensor2_RAW = m_lightsensor2_RAW.getAvg();
        out_frame.temperature1_C = m_temperature1_C.getAvg();
        out_frame.temperature2_C = m_temperature2_C.getAvg();
        out_frame.humidity1_Pct = m_humidity1_Pct.getAvg();
        out_frame.humidity2_Pct = m_humidity2_Pct.getAvg();
        out_frame.pressure1_kPa = m_pressure1_kPa.getAvg();
        out_frame.pressure2_kPa = m_pressure2_kPa.getAvg();
    }

    return out_frame;
}


static void serializeSensors(sensors_data_t frame, char *outBuffer = NULL)
{
    sprintf(outBuffer,
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
        frame.battery_Volt,
        frame.solar_Volt,
        frame.lightsensor1_RAW,
        frame.lightsensor2_RAW,
        frame.temperature1_C,
        frame.temperature2_C,
        frame.humidity1_Pct,
        frame.humidity2_Pct,
        frame.pressure1_kPa,
        frame.pressure2_kPa
    );
}


static void displaySensors(sensors_data_t frame)
{
    Display.printf("Volt: %.2f %.2f\n", frame.battery_Volt, frame.solar_Volt);
    Display.printf("Light: %d %d\n", frame.lightsensor1_RAW, frame.lightsensor2_RAW);
    Display.printf("Temp: %.2f %.2f\n", frame.temperature1_C, frame.temperature2_C);
    Display.printf("Humidity: %.0f %.0f\n", frame.humidity1_Pct, frame.humidity2_Pct);
    Display.printf("Pressure: %.2f %.2f\n", frame.pressure1_kPa, frame.pressure2_kPa);
}


static void displaySensors()
{
    displaySensors(readSensors());
}
