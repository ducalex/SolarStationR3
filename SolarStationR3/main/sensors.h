#include "Adafruit_ADS1015.h"
#include "Adafruit_BME280.h"
#include "BMP180.h"
#include "DHT.h"

#define SENSOR(key, unit, desc, avgr) {key, unit, desc, 1, avgr, 0, 0.00, 0.00, 0.00, 0.00}
typedef struct {
    char key[8];      // Sensor key used when serializing
    char unit[4];     //
    char desc[16];    //
    short status;     // Status
    short nsamples;   // Used in avg calculation
    short count;      //
    float min;        // All time min
    float max;        // All time max
    float avg;        // Average value of last nsamples
    float val;        // Current value
} SENSOR_t;

#define SENSOR_OK 0
#define SENSOR_PENDING 1
#define SENSOR_ERR_UNKNOWN -1
#define SENSOR_ERR_TIMEOUT -2

RTC_DATA_ATTR SENSOR_t SENSORS[] = {
    SENSOR("bat",  "V",    "Battery",     5),
    SENSOR("sol",  "V",    "Solar",      10),
    SENSOR("l1",   "raw",  "Light 1",    10),
    SENSOR("l2",   "raw",  "Light 2",    10),
    SENSOR("t1",   "C",    "Temp 1",     10),
    SENSOR("t2",   "C",    "Temp 2",     10),
    SENSOR("h1",   "%",    "Humidity 1", 10),
    SENSOR("h2",   "%",    "Humidity 2", 10),
    SENSOR("p1",   "kPa",  "Pressure 1", 10),
    SENSOR("p2",   "kPa",  "Pressure 2", 10),
    SENSOR("ws",   "kmh",  "Wind Speed", 10),
    SENSOR("wd",   "deg",  "Wind Dir.",  10),
    SENSOR("rain", "ohm",  "Rain",       10),
};
const int SENSORS_COUNT = (sizeof(SENSORS) / sizeof(SENSOR_t));

extern ConfigProvider config;
float ulp_wind_read_kph();

SENSOR_t *getSensor(const char* key)
{
    for(int i = 0; i < SENSORS_COUNT; i++) {
        if (strcmp(key, SENSORS[i].key) == 0) {
            return &SENSORS[i];
        }
    }
    return NULL;
}


static void setSensorValue(const char *key, float value)
{
    SENSOR_t *handle = getSensor(key);
    if (handle->count + 1 < handle->nsamples)
        handle->count++;
    handle->avg = (handle->avg * ((float)(handle->count - 1) / handle->count)) + value / handle->count;
    handle->val = value;
    handle->status = SENSOR_OK;
    if (value < handle->min || handle->count == 1) handle->min = value;
    if (value > handle->max || handle->count == 1) handle->max = value;
}


static void setSensorError(const char *key, short status = SENSOR_ERR_UNKNOWN)
{
    SENSOR_t *handle = getSensor(key);
    handle->status = status;
    handle->val = 0;
}


void pollSensors()
{
    Adafruit_BME280 bme280;
    Adafruit_ADS1115 ads;
    float t = 0, h = 0, p = 0;

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        setSensorValue("t1", t);
        setSensorValue("h1", h);
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    } else {
        setSensorError("t1", SENSOR_ERR_UNKNOWN);
        setSensorError("h1", SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "DHT sensor not responding");
    }

    if (BMP180_read(BMP180_I2C_ADDR, &t, &p)) {
        setSensorValue("t2", t);
        setSensorValue("p1", p);
        ESP_LOGI(__func__, "BMP: %.2f %.2f", t, p);
    } else {
        setSensorError("t2", SENSOR_ERR_UNKNOWN);
        setSensorError("p1", SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "BMP180 sensor not responding");
    }

    if (bme280.begin()) {
        setSensorValue("t2", t = bme280.readTemperature());
        setSensorValue("h2", h = bme280.readHumidity());
        setSensorValue("p2", p = bme280.readPressure() / 1000);
        ESP_LOGI(__func__, "BME: %.2f %.2f %.2f", t, h, p);
    } else {
        //setSensorError("t2", SENSOR_ERR_UNKNOWN);
        setSensorError("h2", SENSOR_ERR_UNKNOWN);
        setSensorError("p2", SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "BME280 sensor not responding");
    }

    if (ads.begin()) {
        ads.setGain(GAIN_ONE); // real range is vdd + 0.3

        float adc0 = 0, adc1 = 0, adc2 = 0, adc3 = 0;
        float vbit = 0.000125;

        adc0 = (int16_t)ads.readADC_SingleEnded(0) * vbit;
        adc1 = (int16_t)ads.readADC_SingleEnded(1) * vbit;
        adc2 = (int16_t)ads.readADC_SingleEnded(2) * vbit;
        adc3 = (int16_t)ads.readADC_SingleEnded(3) * vbit;
        delay(50); // There seem to be some issue with the ADC, as maxounette foresaw

        setSensorValue("bat", adc0 * CFG_DBL("sensors.adc.adc0_multiplier"));
        setSensorValue("sol", adc1 * CFG_DBL("sensors.adc.adc1_multiplier"));
        setSensorValue("l1",  adc2 * CFG_DBL("sensors.adc.adc2_multiplier"));
        ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f %.2f", adc0, adc1, adc2, adc3);
    } else {
        setSensorError("bat", SENSOR_ERR_UNKNOWN);
        setSensorError("sol", SENSOR_ERR_UNKNOWN);
        setSensorError("l1", SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "ADS1115 sensor not responding");
    }

    float ws = ulp_wind_read_kph();
    float wd = 0;
    setSensorValue("ws", ws);
    setSensorValue("wd", wd);
    ESP_LOGI(__func__, "WIND: %.2f %.2f", ws, wd);
}


void displaySensors()
{
    String content = CFG_STR("STATION.DISPLAY_CONTENT");
    char buffer1[32], buffer2[32];

    for (int i = 0; i < SENSORS_COUNT; i++) {
        for (int d = 0; d < 6; d++) { // That's a very lazy way to do it :S
            if (SENSORS[i].status == 0) {
                sprintf(buffer1, "%%.%df%%s", d);
                sprintf(buffer2, buffer1, SENSORS[i].val, SENSORS[i].unit);
            } else {
                strcpy(buffer2, "ERR");
            }
            sprintf(buffer1, "$%s.%d", SENSORS[i].key, d);
            content.replace(buffer1, buffer2);
        }
    }

    Display.printf("\n%s", content.c_str());
}
