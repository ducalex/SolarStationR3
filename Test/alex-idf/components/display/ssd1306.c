#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "fonts/allFonts.h"

#include "ssd1306.h"

static uint8_t m_i2c_num = 0;
static uint8_t m_i2c_address = 0xC0;
static uint8_t m_col;            // Cursor column.
static uint8_t m_row;            // Cursor RAM row.
static uint8_t m_displayWidth;   // Display width.
static uint8_t m_displayHeight;  // Display height.
static uint8_t m_letterSpacing;  // Letter-spacing in pixels.
static uint8_t m_startLine;      // Top line of display
static uint8_t m_pageOffset;     // Top page of RAM window.
static uint8_t m_scrollMode = 1;  // Scroll mode for newline.
static const uint8_t* m_font = NULL;  // Current font.
static uint8_t m_invertMask = 0;  // font invert mask
static uint8_t m_magFactor = 1;   // Magnification factor.

#define _check ESP_ERROR_CHECK
#define _debug ESP_ERROR_CHECK_WITHOUT_ABORT


static void i2c_fast_write(uint8_t i2c_num, uint8_t address, uint8_t data, uint8_t mode)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    _debug( i2c_master_start(cmd) );
    _debug( i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true) );
    _debug( i2c_master_write_byte(cmd, mode, true) );
    _debug( i2c_master_write_byte(cmd, data, true) );
    _debug( i2c_master_stop(cmd) );
    _debug( i2c_master_cmd_begin(i2c_num, cmd, 10 / portTICK_PERIOD_MS) );
    i2c_cmd_link_delete(cmd);
}

static void ssd1306_writeRam(uint8_t c)
{
    if (m_col >= m_displayWidth) return;
    i2c_fast_write(m_i2c_num, m_i2c_address, c^m_invertMask, SSD1306_MODE_RAM);
    m_col++;
}

static void ssd1306_writeCmd(uint8_t c)
{
    i2c_fast_write(m_i2c_num, m_i2c_address, c, SSD1306_MODE_CMD);
}


void ssd1306_clear()
{

}


void ssd1306_init(const DevType* dev)
{
    m_col = 0;
    m_row = 0;
    const uint8_t* table = dev->initcmds;
    uint8_t size = &dev->initSize;
    m_displayWidth = &dev->lcdWidth;
    m_displayHeight = &dev->lcdHeight;
    for (uint8_t i = 0; i < size; i++) {
        ssd1306_writeCmd(table + i);
    }
    ssd1306_clear();
}


void ssd1306_sleep()
{

}


void ssd1306_deinit()
{

}


void ssd1306_set_row(int row)
{
    if (row >= m_displayHeight) return;
    m_row = row;
    ssd1306_writeCmd(SSD1306_SETSTARTPAGE | ((m_row + m_pageOffset) & 7));
}


void ssd1306_set_col(int col)
{
    if (col >= m_displayWidth) return;
    m_col = col;
    ssd1306_writeCmd(SSD1306_SETLOWCOLUMN | (col & 0XF));
    ssd1306_writeCmd(SSD1306_SETHIGHCOLUMN | (col >> 4));
}


void ssd1306_printf(char *format, ...)
{

}
