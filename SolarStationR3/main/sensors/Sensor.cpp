#include "string.h"
#include "stdio.h"
#include "esp_attr.h"
#include "Arduino.h"
#include "DHT.h"

typedef struct {
    char  description[16];
    char  unit[4];
    float min;
    float max;
    float avg;
    float val;
} SENSOR_ATTR_t;

RTC_DATA_ATTR static SENSOR_ATTR_t rtc_sensor_values[64];

class Sensor
{
  protected:
    short m_driver_arg1; // Driver arg1, typically i2c address or gpio
    short m_driver_arg2; // Driver arg2
    short m_status;      // Status
    long  m_updated;     // Last update timestamp
    short m_nsamples;
    short m_count;
    SENSOR_ATTR_t m_attributes[8];

    void add(int index, float newValue)
    {
        SENSOR_ATTR_t *value = &m_attributes[index];
        if (m_count + 1 < m_nsamples)
            m_count++;
        value->avg = (value->avg * ((float)(m_count - 1) / m_count)) + newValue / m_count;
        value->val = newValue;
        if (newValue < value->min || m_count == 1) value->min = newValue;
        if (newValue > value->max || m_count == 1) value->max = newValue;
    }

  public:
    Sensor(short arg1, short arg2, short maximum)
    {
        m_driver_arg1 = arg1;
        m_driver_arg2 = arg2;
        m_nsamples = maximum;
        reset();
    }

    bool init()
    {

    }

    bool deinit()
    {

    }

    bool read()
    {

    }

    bool reset()
    {
        memset(m_attributes, 0, sizeof(m_attributes));
        m_count = 0;
    }

    const SENSOR_ATTR_t *getAttr(int index)
    {
        return &m_attributes[index];
    }

    short getStatus()
    {
        return m_status;
    }
};

class SensorDHT : public Sensor
{
  using Sensor::Sensor;

  protected:
    SENSOR_ATTR_t m_attributes[8] = {
        {"Temperature", "C", 0, 0, 0, 0},
        {"Humidity", "%", 0, 0, 0, 0},
    };

  public:
    bool read()
    {
        if (m_updated - millis() < 2000) {
            delay(2000 - (m_updated - millis()));
        }

        float t = 0, h = 0;

        if (dht_read(m_driver_arg2, m_driver_arg1, &t, &h)) {
            add(0, t);
            add(1, h);
            m_updated = millis();
            return true;
        }

        return false;
    }
};


void test()
{
    Sensor *abc = new SensorDHT(0, 0, 0);
    abc->init();
    abc->read();
    abc->deinit();
    printf("Temp: %.2f Humidity: %.2f\n", abc->getAttr(0)->val, abc->getAttr(1)->val);
}
