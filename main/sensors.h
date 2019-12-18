#include "Adafruit_ADS1015.h"
#include "Adafruit_BME280.h"
#include "BMP180.h"
#include "DHT.h"

#define SENSOR(key, unit, desc, avgr, attr) {key, unit, desc, 1, avgr, 0, 0.00, 0.00, 0.00, 0.00, attr}
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
    uint8_t attr;     // Linked sensor attribute (4 bits sensor type, 4 bits attribute number)
} SENSOR_t;

#define SENSOR_OK 0
#define SENSOR_PENDING 1
#define SENSOR_ERR_UNKNOWN -1
#define SENSOR_ERR_TIMEOUT -2

#define SENSOR_ATTR_NOT_SET __FLT_MAX__
#define SENSOR_ATTR_PENDING __FLT_MIN__

typedef enum {
    SENSOR_NULL = (0 << 4),
    SENSOR_ADC  = (1 << 4),
    SENSOR_ADS  = (2 << 4),
    SENSOR_BMP  = (3 << 4),
    SENSOR_BME  = (4 << 4),
    SENSOR_DHT  = (5 << 4),
    SENSOR_PIN  = (6 << 4),
    SENSOR_WIND = (7 << 4),
} SENSOR_TYPE_t;

RTC_DATA_ATTR SENSOR_t SENSORS[] = {
    SENSOR("bat",  "V",    "Battery",     5, SENSOR_ADS|0),
    SENSOR("sol",  "V",    "Solar",      10, SENSOR_ADS|1),
    SENSOR("l1",   "raw",  "Light 1",    10, SENSOR_ADS|2),
    SENSOR("l2",   "raw",  "Light 2",    10, SENSOR_ADS|3),
    SENSOR("t1",   "C",    "Temp 1",     10, SENSOR_DHT|0),
    SENSOR("t2",   "C",    "Temp 2",     10, SENSOR_BMP|0), // SENSOR_BME(0)
    SENSOR("h1",   "%",    "Humidity 1", 10, SENSOR_DHT|1),
    SENSOR("h2",   "%",    "Humidity 2", 10, SENSOR_BME|1),
    SENSOR("p1",   "kPa",  "Pressure 1", 10, SENSOR_BMP|1),
    SENSOR("p2",   "kPa",  "Pressure 2", 10, SENSOR_BME|2),
    SENSOR("ws",   "kmh",  "Wind Speed", 10, SENSOR_WIND|0),
    SENSOR("wd",   "deg",  "Wind Dir.",  10, SENSOR_WIND|1),
    SENSOR("rain", "ohm",  "Rain",       10, SENSOR_NULL|0),
};
const int SENSORS_COUNT = (sizeof(SENSORS) / sizeof(SENSOR_t));

extern ConfigProvider config;
float ulp_wind_read_kph();

SENSOR_t *getSensor(const char* key)
{
    for (int i = 0; i < SENSORS_COUNT; i++) {
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
    float attributes[0xFF];
    ARRAY_FILL(attributes, 0, 0xFF, SENSOR_ATTR_NOT_SET);

    for (int i = 0; i < SENSORS_COUNT; i++) {
        uint8_t type = SENSORS[i].attr & 0xF0;
        float a = 0, b = 0, c = 0, d = 0;

        if (attributes[type] == SENSOR_ATTR_NOT_SET) {
            ARRAY_FILL(attributes, type, 0x0F, SENSOR_ATTR_PENDING);

            if (type == SENSOR_DHT) {
                if (dht_read(DHT_TYPE, DHT_PIN, &a, &b)) {
                    attributes[SENSOR_DHT|0] = a;
                    attributes[SENSOR_DHT|1] = b;
                    ESP_LOGI(__func__, "DHT: %.2f %.2f", a, b);
                } else {
                    ESP_LOGE(__func__, "DHT sensor not responding");
                }
            }
            else if (type == SENSOR_BMP) {
                if (BMP180_read(BMP180_I2C_ADDR, &a, &b)) {
                    attributes[SENSOR_BMP|0] = a;
                    attributes[SENSOR_BMP|1] = b;
                    ESP_LOGI(__func__, "BMP: %.2f %.2f", a, b);
                } else {
                    ESP_LOGE(__func__, "BMP180 sensor not responding");
                }
            }
            else if (type == SENSOR_BME) {
                Adafruit_BME280 bme280;
                if (bme280.begin()) {
                    attributes[SENSOR_BME|0] = a = bme280.readTemperature();
                    attributes[SENSOR_BME|1] = b = bme280.readHumidity();
                    attributes[SENSOR_BME|2] = c = bme280.readPressure() / 1000;
                    ESP_LOGI(__func__, "BME: %.2f %.2f %.2f", a, b, c);
                } else {
                    ESP_LOGE(__func__, "BME280 sensor not responding");
                }
            }
            else if (type == SENSOR_ADS) {
                Adafruit_ADS1115 ads;
                float vbit = 0.000125;
                if (ads.begin()) {
                    ads.setGain(GAIN_ONE); // real range is vdd + 0.3
                    attributes[SENSOR_ADS|0] = a = (short)ads.readADC_SingleEnded(0) * vbit * CFG_DBL("sensors.adc.adc0_multiplier");
                    attributes[SENSOR_ADS|1] = b = (short)ads.readADC_SingleEnded(1) * vbit * CFG_DBL("sensors.adc.adc1_multiplier");
                    attributes[SENSOR_ADS|2] = c = (short)ads.readADC_SingleEnded(2) * vbit * CFG_DBL("sensors.adc.adc2_multiplier");
                    attributes[SENSOR_ADS|3] = d = (short)ads.readADC_SingleEnded(3) * vbit * CFG_DBL("sensors.adc.adc3_multiplier");
                    delay(50); // There seem to be some issue with the ADC, as maxounette foresaw
                    ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f %.2f", a, b, c, d);
                } else {
                    ESP_LOGE(__func__, "ADS1115 sensor not responding");
                }
            }
            else if (type == SENSOR_WIND) {
                attributes[SENSOR_WIND|0] = a = ulp_wind_read_kph();
                attributes[SENSOR_WIND|1] = b = 0;
                ESP_LOGI(__func__, "WIND: %.2f %.2f", a, b);
            }
            else if (type == SENSOR_NULL) {
                ARRAY_FILL(attributes, SENSOR_NULL, 0x0F, 0);
                ESP_LOGI(__func__, "NULL: 0 0 0 0 0 0 0 0");
            }
            else {
                ESP_LOGE(__func__, "Unknown sensor 0x%x claimed by '%s'", SENSORS[i].attr, SENSORS[i].key);
            }
        }

        if (attributes[SENSORS[i].attr] != SENSOR_ATTR_PENDING) {
            setSensorValue(SENSORS[i].key, attributes[SENSORS[i].attr]);
        } else {
            setSensorError(SENSORS[i].key, SENSOR_ERR_UNKNOWN);
        }
    }
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

    Display.printf("%s", content.c_str());
}
