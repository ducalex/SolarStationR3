// This code is heavily inspired by Adafruit_BMP085, but given the nature of the
// port (Arduino to esp-idf, C++ to C) very little original code remains.

/**
 * This module supports BMP085, BMP180, and soon BMP280
 */
static const char *MODULE = "BMP180";

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "BMP180.h"


static bool i2c_write(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(MODULE, "You need to initialize the i2c driver first with i2c_driver_install");
    }

    return ret == ESP_OK;
}


static bool i2c_read(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t *out, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_address << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, out, len, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(MODULE, "You need to initialize the i2c driver first with i2c_driver_install");
    }

    return ret == ESP_OK;
}


static uint16_t readInt(bmp180_dev_t *dev, uint8_t a, uint8_t size)
{
    uint8_t ret[4];

    i2c_write(dev->i2c_num, dev->i2c_address, &a, 1);
    i2c_read(dev->i2c_num, dev->i2c_address, &ret, size);

    return (size == 1) ? ret[0] : ((ret[0] << 8) | ret[1]);
}


static uint16_t readRawTemperature(bmp180_dev_t *dev)
{
    uint8_t data[] = {BMP180_REG_CONTROL, BMP180_CMD_READTEMP};
    i2c_write(dev->i2c_num, dev->i2c_address, data, 2);
    usleep(5 * 1000);
    return readInt(dev, BMP180_REG_RESULT, 2);
}


static uint32_t readRawPressure(bmp180_dev_t *dev)
{
    uint32_t raw;

    uint8_t data[] = {BMP180_REG_CONTROL, BMP180_CMD_READPRESSURE + (dev->oversampling << 6)};
    i2c_write(dev->i2c_num, dev->i2c_address, data, 2);

    uint8_t delays[] = {5, 8, 14, 26};
    usleep(delays[dev->oversampling % 4] * 1000);

    raw = readInt(dev, BMP180_REG_RESULT, 2);

    raw <<= 8;
    raw |= readInt(dev, BMP180_REG_RESULT + 2, 1);
    raw >>= (8 - dev->oversampling);

    return raw;
}


bmp180_dev_t* BMP180_init(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t oversampling)
{
    bmp180_dev_t *dev = malloc(sizeof(bmp180_dev_t));

    dev->i2c_address = i2c_address;
    dev->i2c_num = i2c_num;
    dev->oversampling = oversampling;

    /* read calibration data */
    dev->ac1 = readInt(dev, BMP180_CAL_AC1, 2);
    dev->ac2 = readInt(dev, BMP180_CAL_AC2, 2);
    dev->ac3 = readInt(dev, BMP180_CAL_AC3, 2);
    dev->ac4 = readInt(dev, BMP180_CAL_AC4, 2);
    dev->ac5 = readInt(dev, BMP180_CAL_AC5, 2);
    dev->ac6 = readInt(dev, BMP180_CAL_AC6, 2);

    dev->b1 = readInt(dev, BMP180_CAL_B1, 2);
    dev->b2 = readInt(dev, BMP180_CAL_B2, 2);

    dev->mb = readInt(dev, BMP180_CAL_MB, 2);
    dev->mc = readInt(dev, BMP180_CAL_MC, 2);
    dev->md = readInt(dev, BMP180_CAL_MD, 2);

    if (!(dev->ac1 && dev->ac2 && dev->ac3 && dev->ac4 && dev->ac5 && dev->ac6)) {
        free(dev);
        return NULL;
    }

    return dev;
}


float BMP180_readTemperature(bmp180_dev_t *dev)
{
    int32_t UT, X1, X2, B5;     // following ds convention
    float temp;

    UT = readRawTemperature(dev);

    X1 = (UT - (int32_t)dev->ac6) * ((int32_t)dev->ac5) >> 15;
    X2 = ((int32_t)dev->mc << 11) / (X1+(int32_t)dev->md);
    B5 = X1 + X2;
    temp = (B5 + 8) >> 4;
    temp /= 10;

    return temp;
}


float BMP180_readPressure(bmp180_dev_t *dev)
{
    int32_t UT, UP, B3, B5, B6, X1, X2, X3, p;
    uint32_t B4, B7;

    UT = readRawTemperature(dev);
    UP = readRawPressure(dev);

    X1 = (UT - (int32_t)dev->ac6) * ((int32_t)dev->ac5) >> 15;
    X2 = ((int32_t)dev->mc << 11) / (X1+(int32_t)dev->md);
    B5 = X1 + X2;

    // do pressure calcs
    B6 = B5 - 4000;
    X1 = ((int32_t)dev->b2 * ( (B6 * B6)>>12 )) >> 11;
    X2 = ((int32_t)dev->ac2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = ((((int32_t)dev->ac1*4 + X3) << dev->oversampling) + 2) / 4;

    X1 = ((int32_t)dev->ac3 * B6) >> 13;
    X2 = ((int32_t)dev->b1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = ((uint32_t)dev->ac4 * (uint32_t)(X3 + 32768)) >> 15;
    B7 = ((uint32_t)UP - B3) * (uint32_t)( 50000UL >> dev->oversampling );

    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;


    p = p + ((X1 + X2 + (int32_t)3791) >> 4);
    return p;
}


bool BMP180_read(i2c_port_t i2c_num, uint8_t i2c_address, float *temperature, float *pressure)
{
    bmp180_dev_t *dev = BMP180_init(i2c_num, i2c_address, BMP180_STANDARD);

    if (dev == NULL)
        return false;

    *temperature = BMP180_readTemperature(dev);
    *pressure = BMP180_readPressure(dev);

    free(dev);

    return true;
}


float BMP180_sealevel(float pressure, float altitude)
{
	return (pressure / pow(1 - (altitude / 44330.0), 5.255));
}


float BMP180_altitude(float pressure, float sealevelPressure)
{
	return (44330.0 * (1 - pow(pressure / sealevelPressure, 1 / 5.255)));
}
