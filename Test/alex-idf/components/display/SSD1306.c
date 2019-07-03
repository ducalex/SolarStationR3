/**
 * This module supports SSD1306
 */
static const char *MODULE = "SSD1306";

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <stdarg.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "SSD1306.h"
#include "font8x8_basic.h"


static uint8_t m_displayWidth = 128;
static uint8_t m_displayHeight = 64;
static uint8_t m_row = 0;
static uint8_t m_col = 0;
static uint8_t m_pageOffset = 0;
static uint8_t m_startLine = 0;

static uint8_t m_i2c_num;
static uint8_t m_i2c_address;


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

bool ssd1306_command(uint8_t c)
{
    uint8_t data[] = {SSD1306_MODE_CMD_SINGLE, c};
    return i2c_write(m_i2c_num, m_i2c_address, &data, 2);
}


void ssd1306_set_row(uint8_t row)
{
    if (row >= m_displayHeight) return;
    m_row = row;
    ssd1306_command(SSD1306_SETSTARTPAGE | ((m_row + m_pageOffset) & 7));
}


void ssd1306_set_col(uint8_t col)
{
    if (col >= m_displayWidth) return;
    m_col = col;
    ssd1306_command(SSD1306_SETLOWCOLUMN | (col & 0xF));
    ssd1306_command(SSD1306_SETHIGHCOLUMN | (col >> 4));
}


void ssd1306_clearpage(uint8_t page)
{
	uint8_t zero[131] = {0};

    zero[0] = SSD1306_MODE_CMD_SINGLE;
    zero[1] = 0xB0 | page;
    zero[2] = SSD1306_MODE_DATA_STREAM;
    i2c_write(m_i2c_num, m_i2c_address, zero, sizeof zero);
}


void ssd1306_clear()
{
	for (uint8_t i = 0; i < 8; i++) {
        ssd1306_clearpage(i);
	}

    ssd1306_set_col(0);
    ssd1306_set_row(0);
}


bool ssd1306_init(uint8_t i2c_num, uint8_t i2c_address, ssd1306_type_t type)
{
    m_i2c_num = i2c_num;
    m_i2c_address = i2c_address;

    uint8_t data[] = {
        SSD1306_MODE_CMD_STREAM,
        SSD1306_DISPLAYOFF,
        SSD1306_SETDISPLAYCLOCKDIV, 0x80,  // the suggested ratio 0x80
        SSD1306_SETMULTIPLEX, 0x3F,        // ratio 64
        SSD1306_SETDISPLAYOFFSET, 0x0,     // no offset
        SSD1306_SETSTARTLINE | 0x0,        // line #0
        SSD1306_CHARGEPUMP, 0x14,          // internal vcc
        SSD1306_MEMORYMODE, 0x02,          // page mode
        SSD1306_SEGREMAP | 0x1,            // column 127 mapped to SEG0
        SSD1306_COMSCANDEC,                // column scan direction reversed
        SSD1306_SETCOMPINS, 0x12,          // alt COM pins, disable remap
        SSD1306_SETCONTRAST, 0xA0,         // contrast level 0-255
        SSD1306_SETPRECHARGE, 0xF1,        // pre-charge period (1, 15)
        SSD1306_SETVCOMDETECT, 0x40,       // vcomh regulator level
        SSD1306_DISPLAYALLON_RESUME,
        SSD1306_NORMALDISPLAY,
        SSD1306_DISPLAYON
    };

	if (!i2c_write(m_i2c_num, m_i2c_address, data, sizeof data)) {
		ESP_LOGE(MODULE, "OLED configuration failed.");
        return false;
	}

    ssd1306_clear();

    return true;
}


void ssd1306_deinit()
{

}


void ssd1306_sleep()
{
    ssd1306_command(SSD1306_DISPLAYOFF);
}


void ssd1306_contrast(uint8_t contrast)
{
    uint8_t data[] = {
        SSD1306_MODE_CMD_STREAM,
        SSD1306_SETCONTRAST,
        contrast
    };
    return i2c_write(m_i2c_num, m_i2c_address, data, sizeof data);
}


bool ssd1306_printf(const char *format, ...)
{
    char buffer[512];
    va_list argptr;
    va_start(argptr, format);
    vsprintf(buffer, format, argptr);
    va_end(argptr);

	uint16_t text_len = strlen(buffer);

	for (uint8_t i = 0; i < text_len; i++) {
		if (buffer[i] == '\n' || (m_col + 8) >= m_displayWidth) {
            ssd1306_set_col(0);
            if ((m_row + 1) * 8 >= m_displayHeight) {
                //ssd1306_command(SSD1306_SETSTARTLINE | ((++m_startLine * 8) & 0X3F));
                //ssd1306_clearpage(0);
                //ESP_LOGE("SSD", "HAI");
                ssd1306_clear();
            } else {
                ssd1306_set_row(m_row + 1);
            }
		}

        if (buffer[i] != '\n') {
            uint8_t data[9] = {SSD1306_MODE_DATA_STREAM};
            memcpy(&data[1], font8x8_basic_tr[(uint8_t)buffer[i]], 8);
            if (!i2c_write(m_i2c_num, m_i2c_address, data, sizeof data)) {
                return false;
            }
            m_col += 8;
		}
	}

	return true;
}
