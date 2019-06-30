// This code is somewhat inspired by Adafruit_ADS1X15

/**
 * This module supports ADS1015 and ADS1115
 */
static const char *MODULE = "ADS1x15";

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <math.h>
#include "driver/i2c.h"
#include "esp_log.h"

#include "ADS1x15.h"


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


static void writeRegister(ads1x15_dev_t *dev, uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {reg, (value>>8), (value & 0xFF)};
    i2c_write(dev->i2c_num, dev->i2c_address, &data, 3);
}


static uint16_t readRegister(ads1x15_dev_t *dev, uint8_t reg)
{
    uint8_t data[2];
    i2c_write(dev->i2c_num, dev->i2c_address, &reg, 1);
    i2c_read(dev->i2c_num, dev->i2c_address, data, 2);
    return ((data[0] << 8) | data[1]);
}



ads1x15_dev_t ADS1x15_init(i2c_port_t i2c_num, uint8_t i2c_address, ads1x15_type_t type)
{
    ads1x15_dev_t dev = {
        .i2c_num = i2c_num,
        .i2c_address = i2c_address,
        .conversion_delay = (type == ADS1015 ? 1 : 8),
        .bit_shift = (type == ADS1015 ? 4 : 0),
        .gain = ADS_GAIN_ONE
    };

    return dev;
}


/**
 * Read an unsigned value from a single channel or two channels diff
 */
uint16_t ADS1x15_readADC(ads1x15_dev_t *dev, ads1x15_channel_t channel)
{
    // Start with default values
    uint16_t config = ADS1015_REG_CONFIG_CQUE_NONE    | // Disable the comparator (default val)
                      ADS1015_REG_CONFIG_CLAT_NONLAT  | // Non-latching (default val)
                      ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
                      ADS1015_REG_CONFIG_CMODE_TRAD   | // Traditional comparator (default val)
                      ADS1015_REG_CONFIG_DR_1600SPS   | // 1600 samples per second (default)
                      ADS1015_REG_CONFIG_MODE_SINGLE  | // Single-shot mode (default)
                      ADS1015_REG_CONFIG_OS_SINGLE    | // Start single-conversion
                      dev->gain                       | // Set PGA/voltage range
                      channel                         ; // Set single-ended input channel

    // Write config register to the ADC
    writeRegister(dev, ADS1015_REG_POINTER_CONFIG, config);

    // Wait for the conversion to complete
    usleep(dev->conversion_delay * 1000);

    // Read the conversion results
    // Shift 12-bit results right 4 bits for the ADS1015
    return readRegister(dev, ADS1015_REG_POINTER_CONVERT) >> dev->bit_shift;
}


/**
 * This is the same as ADS1x15_readADC except it returns a signed value
 */
int16_t ADS1x15_readADC_Differential(ads1x15_dev_t *dev, ads1x15_channel_t channels)
{
    uint16_t res = ADS1x15_readADC(dev, channels);
    // Restore sign of 12bit value
    if (dev->bit_shift != 0) {
        if (res > 0x07FF) {
            res |= 0xF000;
        }
    }
    return (int16_t)res;
}


/**************************************************************************/
/*!
    @brief  Sets up the comparator to operate in basic mode, causing the
            ALERT/RDY pin to assert (go from high to low) when the ADC
            value exceeds the specified threshold.
            This will also set the ADC in continuous conversion mode.
*/
/**************************************************************************/
void ADS1x15_startComparator(ads1x15_dev_t *dev, ads1x15_channel_t channel, int16_t threshold)
{
    uint16_t config = ADS1015_REG_CONFIG_CQUE_1CONV   | // Comparator enabled and asserts on 1 match
                      ADS1015_REG_CONFIG_CLAT_LATCH   | // Latching mode
                      ADS1015_REG_CONFIG_CPOL_ACTVLOW | // Alert/Rdy active low   (default val)
                      ADS1015_REG_CONFIG_CMODE_TRAD   | // Traditional comparator (default val)
                      ADS1015_REG_CONFIG_DR_1600SPS   | // 1600 samples per second (default)
                      ADS1015_REG_CONFIG_MODE_CONTIN  | // Continuous conversion mode
                      dev->gain                       | // Set PGA/voltage range
                      channel                         ; // Set single-ended input channel

    // Set the high threshold register
    // Shift 12-bit results left 4 bits for the ADS1015
    writeRegister(dev, ADS1015_REG_POINTER_HITHRESH, threshold << dev->bit_shift);

    // Write config register to the ADC
    writeRegister(dev, ADS1015_REG_POINTER_CONFIG, config);
}


int16_t ADS1x15_getLastConversionResults(ads1x15_dev_t *dev)
{
    // Wait for the conversion to complete
    usleep(dev->conversion_delay * 1000);

    // Read the conversion results
    uint16_t res = readRegister(dev, ADS1015_REG_POINTER_CONVERT) >> dev->bit_shift;
    // Restore sign of 12bit value
    if (dev->bit_shift != 0) {
        if (res > 0x07FF) {
            res |= 0xF000;
        }
    }
    return (int16_t)res;
}
