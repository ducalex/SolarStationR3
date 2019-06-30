// Weather Station 9000 i2c helper

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "esp_err.h"

static bool i2c_fast_init(i2c_port_t i2c_num, uint8_t sdaPin, uint8_t sclPin, uint32_t frequency)
{
	i2c_config_t i2c_config;
    i2c_config.mode = I2C_MODE_MASTER;
    i2c_config.sda_io_num = (gpio_num_t)sdaPin;
    i2c_config.scl_io_num = (gpio_num_t)sclPin;
    i2c_config.sda_pullup_en = GPIO_PULLUP_ENABLE;
    i2c_config.scl_pullup_en = GPIO_PULLUP_ENABLE,
    i2c_config.master.clk_speed = frequency;

	i2c_param_config(i2c_num, &i2c_config);
	esp_err_t ret = i2c_driver_install(i2c_num, I2C_MODE_MASTER, 0, 0, 0);
    //_debug( i2c_set_timeout(i2c_num, I2C_APB_CLK_FREQ * 10) ); // 10 seconds

    if (ret == ESP_OK) {
        ESP_LOGI(__func__, "I2C driver initialized");
    } else {
        ESP_LOGE(__func__, "I2C driver failed to initialized (%s)", esp_err_to_name(ret));
    }

    return (ret == ESP_OK);
}


static void i2c_fast_deinit(i2c_port_t i2c_num)
{
    i2c_driver_delete(i2c_num);
}


static bool i2c_fast_write(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t *data, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 10 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(__func__, "You need to initialize the i2c driver first with i2c_fast_init");
    }
    else if (ret != ESP_OK) {
        ESP_LOGE(__func__, "Error: %s", esp_err_to_name(ret));
    }

    return (ret == ESP_OK);
}


static bool i2c_fast_read(i2c_port_t i2c_num, uint8_t i2c_address, uint8_t *out, uint8_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_address << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, out, len, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(__func__, "You need to initialize the i2c driver first with i2c_fast_init");
    }
    else if (ret != ESP_OK) {
        ESP_LOGE(__func__, "Error: %s", esp_err_to_name(ret));
    }

    return (ret == ESP_OK);
}


static bool i2c_fast_write_byte(i2c_port_t i2c_num, uint8_t address, uint8_t data)
{
    return i2c_fast_write(i2c_num, address, &data, 1);
}


static bool i2c_fast_read_byte(i2c_port_t i2c_num, uint8_t address, uint8_t data)
{
    return i2c_fast_read(i2c_num, address, &data, 1);
}
