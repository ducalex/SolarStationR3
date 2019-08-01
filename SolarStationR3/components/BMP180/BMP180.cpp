/**
 * This module supports BMP085, BMP180, and soon BMP280
 * Loosely based on Adafruit_BMP085
 */
static const char *MODULE = "BMP180";

#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "Wire.h"

#include "BMP180.h"


static bool writeReg(uint8_t i2c_address, uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(i2c_address);
    Wire.write(reg);
    if (val > 0) Wire.write(val);
    return Wire.endTransmission() > 0;
}


static uint16_t readInt(uint8_t i2c_address, uint8_t reg, uint8_t size)
{
    uint8_t ret[4];

    writeReg(i2c_address, reg, 0);

    Wire.requestFrom(i2c_address, size);
    Wire.readBytes((uint8_t*)&ret, size);

    return (size == 1) ? ret[0] : ((ret[0] << 8) | ret[1]);
}


static uint16_t readRawTemperature(bmp180_dev_t *dev)
{
    writeReg(dev->i2c_address, BMP180_REG_CONTROL, BMP180_CMD_READTEMP);
    usleep(5 * 1000);
    return readInt(dev->i2c_address, BMP180_REG_RESULT, 2);
}


static uint32_t readRawPressure(bmp180_dev_t *dev)
{
    uint32_t raw;

    writeReg(dev->i2c_address, BMP180_REG_CONTROL,
        (uint8_t)(BMP180_CMD_READPRESSURE + (dev->oversampling << 6)));

    uint8_t delays[] = {5, 8, 14, 26};
    usleep(delays[dev->oversampling % 4] * 1000);

    raw = (readInt(dev->i2c_address, BMP180_REG_RESULT, 2) << 8)
            | readInt(dev->i2c_address, BMP180_REG_RESULT + 2, 1);
    raw >>= (8 - dev->oversampling);

    return raw;
}


bmp180_dev_t* BMP180_init(uint8_t i2c_address, uint8_t oversampling)
{
    if (!Wire.begin() || readInt(i2c_address, BMP180_CMD_WHO_AM_I, 1) != 0x55) {
        return NULL;
    }

    bmp180_dev_t *dev = (bmp180_dev_t*)malloc(sizeof(bmp180_dev_t));

    dev->i2c_address = i2c_address;
    dev->oversampling = oversampling;

    /* read calibration data */
    dev->ac1 = readInt(i2c_address, BMP180_CAL_AC1, 2);
    dev->ac2 = readInt(i2c_address, BMP180_CAL_AC2, 2);
    dev->ac3 = readInt(i2c_address, BMP180_CAL_AC3, 2);
    dev->ac4 = readInt(i2c_address, BMP180_CAL_AC4, 2);
    dev->ac5 = readInt(i2c_address, BMP180_CAL_AC5, 2);
    dev->ac6 = readInt(i2c_address, BMP180_CAL_AC6, 2);
    dev->b1 = readInt(i2c_address, BMP180_CAL_B1, 2);
    dev->b2 = readInt(i2c_address, BMP180_CAL_B2, 2);
    dev->mb = readInt(i2c_address, BMP180_CAL_MB, 2);
    dev->mc = readInt(i2c_address, BMP180_CAL_MC, 2);
    dev->md = readInt(i2c_address, BMP180_CAL_MD, 2);

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
    B7 = ((uint32_t)UP - B3) * (uint32_t)(50000UL >> dev->oversampling);

    if (B7 < 0x80000000) {
        p = (B7 * 2) / B4;
    } else {
        p = (B7 / B4) * 2;
    }
    X1 = (p >> 8) * (p >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * p) >> 16;


    p += ((X1 + X2 + (int32_t)3791) >> 4);
    return (float)p / 1000;
}


bool BMP180_read(uint8_t i2c_address, float *temperature, float *pressure)
{
    bmp180_dev_t *dev = BMP180_init(i2c_address, BMP180_HIGHRES);

    if (dev == NULL)
        return false;

    *temperature = BMP180_readTemperature(dev);
    *pressure = BMP180_readPressure(dev);

    free(dev);

    return true;
}
