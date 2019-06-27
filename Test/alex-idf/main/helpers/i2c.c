// Weather Station 9000 i2c helper

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "esp_err.h"

#define _check ESP_ERROR_CHECK
#define _debug ESP_ERROR_CHECK_WITHOUT_ABORT


void i2c_init(uint8_t i2c_num, uint8_t sdaPin, uint8_t sclPin, uint32_t frequency)
{
	i2c_config_t i2c_config;
    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = (gpio_num_t)sdaPin;
    i2c_config.scl_io_num = (gpio_num_t)sclPin;
    i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE,
    i2c_config.master.clk_speed = frequency;

	_debug( i2c_param_config(i2c_num, &i2c_config) );
	_debug( i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0) );
    _debug( i2c_set_timeout(i2c_num, I2C_APB_CLK_FREQ * 10) ); // 10 seconds
}


void i2c_deinit(uint8_t i2c_num)
{
    _debug( i2c_driver_delete(i2c_num) );
}


void i2c_fast_write(uint8_t i2c_num, uint8_t address, uint8_t *data, size_t data_len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    _debug( i2c_master_start(cmd) );
    _debug( i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true) );

    if (data_len == 1) {
        _debug( i2c_master_write_byte(cmd, data[0], true) );
    } else {
        _debug( i2c_master_write(cmd, data, data_len, true) );
    }

    _debug( i2c_master_stop(cmd) );
    _debug( i2c_master_cmd_begin(i2c_num, cmd, 10 / portTICK_PERIOD_MS) );
    i2c_cmd_link_delete(cmd);
}


void i2c_fast_write_byte(uint8_t i2c_num, uint8_t address, uint8_t data)
{
    i2c_fast_write(i2c_num, address, &data, 1);
}


void i2c_fast_read(uint8_t i2c_num, uint8_t address, uint8_t *data, size_t data_len)
{
    esp_err_t ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    _debug( i2c_master_start(cmd) );
    _debug( i2c_master_write_byte(cmd, address << 1 | I2C_MASTER_READ, true) );
    if (data_len == 1) {
        _debug( i2c_master_read_byte(cmd, data, true) );
    } else {
        _debug( i2c_master_read(cmd, data, data_len, true) );
    }
    _debug( i2c_master_stop(cmd) );
    _debug( i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS) );
    i2c_cmd_link_delete(cmd);
}


void i2c_fast_read_byte(uint8_t i2c_num, uint8_t address, uint8_t *data)
{
    i2c_fast_read(i2c_num, address, &data, 1);
}
