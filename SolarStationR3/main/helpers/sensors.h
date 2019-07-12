#include "Adafruit_ADS1015.h"
#include "BMP180.h"
#include "DHT.h"
#include "avgrc.h"
#include "../config.h"

static RTC_DATA_ATTR AVGr_t m_battery_Volt      = AVGr(10);
static RTC_DATA_ATTR AVGr_t m_solar_Volt        = AVGr(10);
static RTC_DATA_ATTR AVGr_t m_lightsensor1_RAW  = AVGr(20);
static RTC_DATA_ATTR AVGr_t m_lightsensor2_RAW  = AVGr(20);
static RTC_DATA_ATTR AVGr_t m_temperature1_C    = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_temperature2_C    = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_humidity1_Pct     = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_humidity2_Pct     = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_pressure1_kPa     = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_pressure2_kPa     = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_windSpeed_kmh     = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_windDirection_deg = AVGr(1);
static RTC_DATA_ATTR AVGr_t m_rain_RAW          = AVGr(1);

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
    float t = 0, h = 0, p = 0;

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        avgr_add(&m_temperature1_C, t);
        avgr_add(&m_humidity1_Pct, h);
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    } else {
        // add 0?
        ESP_LOGE(__func__, "DHT sensor not responding");
    }

    if (BMP180_read(BMP180_I2C_ADDR, &t, &p)) {
        avgr_add(&m_temperature2_C, t);
        avgr_add(&m_pressure1_kPa, p);
        ESP_LOGI(__func__, "BMP: %.2f %.2f", t, p);
    } else {
        // add 0?
        ESP_LOGE(__func__, "BMP180 sensor not responding");
    }

    Adafruit_ADS1115 ads;
    ads.begin();
    ads.setGain(GAIN_ONE); // real range is vdd + 0.3

    // The previous sensors are quite stable, ADC readings are less so.
    for (int i = 0; i < 5; i++) {
        avgr_add(&m_battery_Volt, 0.000125 * (int16_t)ads.readADC_SingleEnded(0) * POWER_VBAT_MULTIPLIER);
        avgr_add(&m_solar_Volt, 0.000125 * (int16_t)ads.readADC_SingleEnded(1));
        avgr_add(&m_lightsensor1_RAW, 0.000125 * (int16_t)ads.readADC_SingleEnded(2));
        delay(50);
    }

    ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f",
        m_battery_Volt.val, m_solar_Volt.val, m_lightsensor1_RAW.val);
}


static sensors_data_t readSensors(bool getCurrentValue = false)
{
    sensors_data_t out_frame;

    if (getCurrentValue) {
        out_frame.battery_Volt = m_battery_Volt.val;
        out_frame.solar_Volt = m_solar_Volt.val;
        out_frame.lightsensor1_RAW = m_lightsensor1_RAW.val;
        out_frame.lightsensor2_RAW = m_lightsensor2_RAW.val;
        out_frame.temperature1_C = m_temperature1_C.val;
        out_frame.temperature2_C = m_temperature2_C.val;
        out_frame.humidity1_Pct = m_humidity1_Pct.val;
        out_frame.humidity2_Pct = m_humidity2_Pct.val;
        out_frame.pressure1_kPa = m_pressure1_kPa.val;
        out_frame.pressure2_kPa = m_pressure2_kPa.val;
    } else {
        out_frame.battery_Volt = m_battery_Volt.avg;
        out_frame.solar_Volt = m_solar_Volt.avg;
        out_frame.lightsensor1_RAW = m_lightsensor1_RAW.avg;
        out_frame.lightsensor2_RAW = m_lightsensor2_RAW.avg;
        out_frame.temperature1_C = m_temperature1_C.avg;
        out_frame.temperature2_C = m_temperature2_C.avg;
        out_frame.humidity1_Pct = m_humidity1_Pct.avg;
        out_frame.humidity2_Pct = m_humidity2_Pct.avg;
        out_frame.pressure1_kPa = m_pressure1_kPa.avg;
        out_frame.pressure2_kPa = m_pressure2_kPa.avg;
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
