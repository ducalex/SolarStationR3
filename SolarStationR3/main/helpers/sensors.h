#include "Adafruit_BMP085.h"
#include "Adafruit_ADS1015.h"
#include "DHT.h"
#include "../config.h"


static void readSensors()
{
    Adafruit_BMP085 bmp;
    float t, h, p;

    if (bmp.begin()) {
        ESP_LOGI(__func__, "BMP: %.2f %d", bmp.readTemperature(), bmp.readPressure());
    }

    if (dht_read(DHT_TYPE, DHT_PIN, &t, &h)) {
        ESP_LOGI(__func__, "DHT: %.2f %.2f", t, h);
    }
}
