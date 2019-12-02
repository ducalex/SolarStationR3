#define BMP180_I2C_ADDR 0x77

#define BMP180_CAL_AC1           0xAA  // 16 bits
#define BMP180_CAL_AC2           0xAC  // 16 bits
#define BMP180_CAL_AC3           0xAE  // 16 bits
#define BMP180_CAL_AC4           0xB0  // 16 bits
#define BMP180_CAL_AC5           0xB2  // 16 bits
#define BMP180_CAL_AC6           0xB4  // 16 bits
#define BMP180_CAL_B1            0xB6  // 16 bits
#define BMP180_CAL_B2            0xB8  // 16 bits
#define BMP180_CAL_MB            0xBA  // 16 bits
#define BMP180_CAL_MC            0xBC  // 16 bits
#define BMP180_CAL_MD            0xBE  // 16 bits

#define BMP180_REG_CONTROL       0xF4
#define BMP180_REG_RESULT        0xF6
#define BMP180_CMD_READTEMP      0x2E // 16 bits
#define BMP180_CMD_READPRESSURE  0x34 // 24 bits
#define BMP180_CMD_WHO_AM_I      0xD0 // 8 bits (should return 0x55)
#define BMP180_CMD_RESET         0xE0

enum {
    BMP180_ULTRALOWPOWER = 0,
    BMP180_STANDARD = 1,
    BMP180_HIGHRES = 2,
    BMP180_ULTRAHIGHRES = 3
};


typedef struct {
    uint8_t oversampling;
    uint8_t i2c_address;
    uint8_t who;
    int16_t ac1, ac2, ac3, b1, b2, mb, mc, md;
    uint16_t ac4, ac5, ac6;
} bmp180_dev_t;


// This is a shortcut that inits, reads, deinits the sensor.
bool BMP180_read(uint8_t i2c_address, float *temperature, float *pressure);

bmp180_dev_t* BMP180_init(uint8_t i2c_address, uint8_t oversampling);
float BMP180_readPressure(bmp180_dev_t *dev);
float BMP180_readTemperature(bmp180_dev_t *dev);
