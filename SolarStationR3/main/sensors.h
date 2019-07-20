#include "Adafruit_ADS1015.h"
#include "BMP180.h"
#include "DHT.h"

#define SENSOR(key, unit, desc, avgr) {key, unit, desc, 0, 0, avgr, 0, 0.00, 0.00, 0.00, 0.00}
typedef struct {
    char key[8];      // Sensor key used when serializing
    char unit[4];     //
    char desc[16];    //
    short status;     // Status
    unsigned int updated;    // Last update timestamp
    short nsamples;   // Used in avg calculation
    short count;      //
    float min;        // All time min
    float max;        // All time max
    float avg;        // Average value of last nsamples
    float val;        // Current value
} SENSOR_t;

#define SENSOR_OK 0
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
uint32_t rtc_millis();
int ulp_wind_read();

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
    handle->updated = rtc_millis();
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

    Wire.beginTransmission(ADS1015_ADDRESS);

    if (Wire.endTransmission() == 0) {
        Adafruit_ADS1115 ads;
        ads.begin();
        ads.setGain(GAIN_ONE); // real range is vdd + 0.3

        float adc0 = 0, adc1 = 0, adc2 = 0, adc3 = 0;
        float vbit = 0.000125;

        adc0 = (int16_t)ads.readADC_SingleEnded(0) * vbit;
        adc1 = (int16_t)ads.readADC_SingleEnded(1) * vbit;
        adc2 = (int16_t)ads.readADC_SingleEnded(2) * vbit;
        adc3 = (int16_t)ads.readADC_SingleEnded(3) * vbit;

        setSensorValue("bat", adc0 * CFG_DBL("POWER.VBAT_MULTIPLIER"));
        setSensorValue("sol", adc1);
        setSensorValue("l1", adc2);
        ESP_LOGI(__func__, "ADC: %.2f %.2f %.2f %.2f", adc0, adc1, adc2, adc3);
    } else {
        setSensorError("bat", SENSOR_ERR_UNKNOWN);
        setSensorError("sol", SENSOR_ERR_UNKNOWN);
        setSensorError("l1", SENSOR_ERR_UNKNOWN);
        ESP_LOGE(__func__, "ADS1115 sensor not responding");
    }

    float circ = (2 * 3.141592 * CFG_DBL("sensors.anemometer.radius")) / 100 / 1000;
    float ws = ulp_wind_read() * 60 * circ * CFG_DBL("sensors.anemometer.calibration");
    float wd = 0;
    setSensorValue("ws", ws);
    setSensorValue("wd", wd);
    ESP_LOGI(__func__, "WIND: %d %d", (int)ws, (int)wd);
}


void packSensors(float *outFrame, uint32_t *outStatus)
{
    *outStatus = 0;

    for (int i = 0; i < SENSORS_COUNT; i++) {
        outFrame[i] = SENSORS[i].val;
        if (SENSORS[i].status != 0) {
            *outStatus |= (1 << i);
        }
    }
}


char* serializeSensors(float *frame)
{
    char *outBuffer = (char*)calloc(SENSORS_COUNT * 16, 1);
    for (int i = 0; i < SENSORS_COUNT; i++) {
        sprintf(outBuffer + strlen(outBuffer), "&%s=%.4f", SENSORS[i].key, frame[i]);
    }
    return outBuffer;
}


void displaySensors()
{
    #define P_SENS(key) getSensor(key)->val, getSensor(key)->unit
    Display.printf("\nVolt: %.2f%s %.2f%s", P_SENS("bat"), P_SENS("sol"));
    Display.printf("\nLight: %.0f%s %.0f%s", P_SENS("l1"), P_SENS("l2"));
    Display.printf("\nTemp: %.2f%s %.2f%s", P_SENS("t1"), P_SENS("t2"));
    Display.printf("\nHumidity: %.0f%s %.0f%s", P_SENS("h1"), P_SENS("h2"));
    Display.printf("\nPres: %.2f%s %.2f%s", P_SENS("p1"), P_SENS("p2"));
    Display.printf("\nWind: %.2f%s %.2f%s", P_SENS("ws"), P_SENS("wd"));
}
