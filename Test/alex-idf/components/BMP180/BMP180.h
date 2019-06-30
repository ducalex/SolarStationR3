#include <stdbool.h>
#include "driver/i2c.h"

#define BMP180_I2CADDR 0x77

#define BMP180_ULTRALOWPOWER 0
#define BMP180_STANDARD      1
#define BMP180_HIGHRES       2
#define BMP180_ULTRAHIGHRES  3
#define BMP180_CAL_AC1           0xAA  // R   Calibration data (16 bits)
#define BMP180_CAL_AC2           0xAC  // R   Calibration data (16 bits)
#define BMP180_CAL_AC3           0xAE  // R   Calibration data (16 bits)
#define BMP180_CAL_AC4           0xB0  // R   Calibration data (16 bits)
#define BMP180_CAL_AC5           0xB2  // R   Calibration data (16 bits)
#define BMP180_CAL_AC6           0xB4  // R   Calibration data (16 bits)
#define BMP180_CAL_B1            0xB6  // R   Calibration data (16 bits)
#define BMP180_CAL_B2            0xB8  // R   Calibration data (16 bits)
#define BMP180_CAL_MB            0xBA  // R   Calibration data (16 bits)
#define BMP180_CAL_MC            0xBC  // R   Calibration data (16 bits)
#define BMP180_CAL_MD            0xBE  // R   Calibration data (16 bits)

#define BMP180_REG_CONTROL       0xF4
#define BMP180_REG_RESULT        0xF6
#define BMP180_CMD_READTEMP      0x2E
#define BMP180_CMD_READPRESSURE  0x34


typedef struct {
    uint8_t oversampling;
    uint8_t i2c_num;
    uint8_t i2c_address;
    int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
    uint16_t ac4, ac5, ac6;
} bmp180_dev_t;


// This is a shortcut that inits, reads, deinits the sensor.
bool BMP180_read(i2c_port_t i2c_num, uint8_t i2c_address, float *temperature, float *pressure);

bmp180_dev_t* BMP180_init(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t oversampling);
float BMP180_readPressure(bmp180_dev_t *dev);
float BMP180_readTemperature(bmp180_dev_t *dev);

float BMP180_sealevel(float pressure, float altitude);
float BMP180_altitude(float pressure, float sealevelPressure);
