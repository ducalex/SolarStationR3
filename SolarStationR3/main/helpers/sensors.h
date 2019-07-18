#include "Adafruit_ADS1015.h"
#include "BMP180.h"
#include "DHT.h"
#include "../config.h"

#define SENSOR(key, avgr) {key, 0, 0, avgr, 0, 0.00, 0.00, 0.00, 0.00}
typedef struct {
    char key[8];      // Sensor key used when serializing
    short status;     // Status
    ulong updated;    // Last update timestamp
    short nsamples;   // Used in avg calculation
    short count;      //
    float min;        // All time min
    float max;        // All time max
    float avg;        // Average value of last nsamples
    float val;        // Current value
} SENSOR_t;

static RTC_DATA_ATTR SENSOR_t m_battery_Volt      = SENSOR("bat", 10);
static RTC_DATA_ATTR SENSOR_t m_solar_Volt        = SENSOR("sol", 10);
static RTC_DATA_ATTR SENSOR_t m_lightsensor1_RAW  = SENSOR("l1", 10);
static RTC_DATA_ATTR SENSOR_t m_lightsensor2_RAW  = SENSOR("l2", 10);
static RTC_DATA_ATTR SENSOR_t m_temperature1_C    = SENSOR("t1", 10);
static RTC_DATA_ATTR SENSOR_t m_temperature2_C    = SENSOR("t2", 10);
static RTC_DATA_ATTR SENSOR_t m_humidity1_Pct     = SENSOR("h1", 10);
static RTC_DATA_ATTR SENSOR_t m_humidity2_Pct     = SENSOR("h2", 10);
static RTC_DATA_ATTR SENSOR_t m_pressure1_kPa     = SENSOR("p1", 10);
static RTC_DATA_ATTR SENSOR_t m_pressure2_kPa     = SENSOR("p2", 10);
static RTC_DATA_ATTR SENSOR_t m_windSpeed_kmh     = SENSOR("ws", 10);
static RTC_DATA_ATTR SENSOR_t m_windDirection_deg = SENSOR("wd", 10);
static RTC_DATA_ATTR SENSOR_t m_rain_RAW          = SENSOR("rain", 10);

const SENSOR_t *sensors[] = {
    &m_battery_Volt,
    &m_solar_Volt,
    &m_lightsensor1_RAW,
    &m_lightsensor2_RAW,
    &m_temperature1_C,
    &m_temperature2_C,
    &m_humidity1_Pct,
    &m_humidity2_Pct,
    &m_pressure1_kPa,
    &m_pressure2_kPa,
    &m_windSpeed_kmh,
    &m_windDirection_deg,
    &m_rain_RAW
};
const int SENSORS_COUNT = (sizeof(sensors) / sizeof(SENSOR_t*));

#define SENSOR_OK 0
#define SENSOR_ERR_UNKNOWN -1
#define SENSOR_ERR_TIMEOUT -2

static void setSensorValue(SENSOR_t *handle, float value)
{
    if (handle->count + 1 < handle->nsamples)
        handle->count++;
    handle->avg = (handle->avg * ((float)(handle->count - 1) / handle->count)) + value / handle->count;
    handle->val = value;
    handle->status = SENSOR_OK;
    handle->updated = rtc_millis();
    if (value < handle->min || handle->count == 1) handle->min = value;
    if (value > handle->max || handle->count == 1) handle->max = value;
}


static void setSensorError(SENSOR_t *handle, short status = SENSOR_ERR_UNKNOWN)
{
    handle->status = status;
    handle->val = 0;
}


static void pollSensors()
{
    float t = 0, h = 0, p = 0;

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        setSensorValue(&m_temperature1_C, t);
        setSensorValue(&m_humidity1_Pct, h);
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    } else {
        setSensorError(&m_temperature1_C, SENSOR_ERR_UNKNOWN);
        setSensorError(&m_humidity1_Pct, SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "DHT sensor not responding");
    }

    if (BMP180_read(BMP180_I2C_ADDR, &t, &p)) {
        setSensorValue(&m_temperature2_C, t);
        setSensorValue(&m_pressure1_kPa, p);
        ESP_LOGI(__func__, "BMP: %.2f %.2f", t, p);
    } else {
        setSensorError(&m_temperature2_C, SENSOR_ERR_UNKNOWN);
        setSensorError(&m_pressure1_kPa, SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "BMP180 sensor not responding");
    }

    Wire.beginTransmission(ADS1015_ADDRESS);

    if (Wire.endTransmission() == 0) {
        Adafruit_ADS1115 ads;
        ads.begin();
        ads.setGain(GAIN_ONE); // real range is vdd + 0.3

        float adc0 = 0, adc1 = 0, adc2 = 0, adc3 = 0;
        float vbit = 0.000125, samples = 10;

        adc0 = (int16_t)ads.readADC_SingleEnded(0) * vbit;
        adc1 = (int16_t)ads.readADC_SingleEnded(1) * vbit;
        adc2 = (int16_t)ads.readADC_SingleEnded(2) * vbit;
        adc3 = (int16_t)ads.readADC_SingleEnded(3) * vbit;

        setSensorValue(&m_battery_Volt, adc0 * POWER_VBAT_MULTIPLIER);
        setSensorValue(&m_solar_Volt, adc1);
        setSensorValue(&m_lightsensor1_RAW, adc2);
        ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f %.2f", adc0, adc1, adc2, adc3);
    } else {
        setSensorError(&m_battery_Volt, SENSOR_ERR_UNKNOWN);
        setSensorError(&m_solar_Volt, SENSOR_ERR_UNKNOWN);
        setSensorError(&m_lightsensor1_RAW, SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "ADS1115 sensor not responding");
    }

    float ws = ulp_wind_read();
    float wd = 0;
    setSensorValue(&m_windSpeed_kmh, ws);
    setSensorValue(&m_windDirection_deg, wd);
    ESP_LOGI(__func__, "WIND: %d %d", (int)ws, (int)wd);
}


static void packSensors(float *outFrame, uint32_t *outStatus)
{
    *outStatus = 0;

    for (int i = 0; i < SENSORS_COUNT; i++) {
        outFrame[i] = sensors[i]->val;
        if (sensors[i]->status != 0) {
            *outStatus |= (1 << i);
        }
    }
}


static char* serializeSensors(float *frame)
{
    char *outBuffer = (char*)calloc(SENSORS_COUNT * 16, 1);
    for (int i = 0; i < SENSORS_COUNT; i++) {
        sprintf(outBuffer + strlen(outBuffer), "&%s=%.4f", sensors[i]->key, frame[i]);
    }
    return outBuffer;
}


static void displaySensors()
{
    Display.printf("\nVolt: %.2f %.2f", m_battery_Volt.val, m_solar_Volt.val);
    Display.printf("\nLight: %d %d", (int16_t)m_lightsensor1_RAW.val, (int16_t)m_lightsensor2_RAW.val);
    Display.printf("\nTemp: %.2f %.2f", m_temperature1_C.val, m_temperature2_C.val);
    Display.printf("\nHumidity: %.0f %.0f", m_humidity1_Pct.val, m_humidity2_Pct.val);
    Display.printf("\nPressure: %.2f %.2f", m_pressure1_kPa.val, m_pressure2_kPa.val);
    Display.printf("\nWind: %d %d", (int)m_windSpeed_kmh.val, (int)m_windDirection_deg.val);
}
