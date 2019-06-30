#pragma once
#include <stdint.h>

typedef enum
{
    SSD1306_128x64 = 0,
    SSD1306_128x32 = 1
} ssd1306_type_t;

#define SSD1306_I2C_ADDRESS   0x3C

#define SSD1306_MODE_CMD_STREAM     0X00
#define SSD1306_MODE_CMD_SINGLE     0X80
#define SSD1306_MODE_DATA_STREAM    0X40


/** Set Lower Column Start Address for Page Addressing Mode. */
#define SSD1306_SETLOWCOLUMN 0x00
/** Set Higher Column Start Address for Page Addressing Mode. */
#define SSD1306_SETHIGHCOLUMN 0x10
/** Set Memory Addressing Mode. */
#define SSD1306_MEMORYMODE 0x20
/** Set display RAM display start line register from 0 - 63. */
#define SSD1306_SETSTARTLINE 0x40
/** Set Display Contrast to one of 256 steps. */
#define SSD1306_SETCONTRAST 0x81
/** Enable or disable charge pump.  Follow with 0X14 enable, 0X10 disable. */
#define SSD1306_CHARGEPUMP 0x8D
/** Set Segment Re-map between data column and the segment driver. */
#define SSD1306_SEGREMAP 0xA0
/** Resume display from GRAM content. */
#define SSD1306_DISPLAYALLON_RESUME 0xA4
/** Force display on regardless of GRAM content. */
#define SSD1306_DISPLAYALLON 0xA5
/** Set Normal Display. */
#define SSD1306_NORMALDISPLAY 0xA6
/** Set Inverse Display. */
#define SSD1306_INVERTDISPLAY 0xA7
/** Set Multiplex Ratio from 16 to 63. */
#define SSD1306_SETMULTIPLEX 0xA8
/** Set Display off. */
#define SSD1306_DISPLAYOFF 0xAE
/** Set Display on. */
#define SSD1306_DISPLAYON 0xAF
/**Set GDDRAM Page Start Address. */
#define SSD1306_SETSTARTPAGE 0XB0
/** Set COM output scan direction normal. */
#define SSD1306_COMSCANINC 0xC0
/** Set COM output scan direction reversed. */
#define SSD1306_COMSCANDEC 0xC8
/** Set Display Offset. */
#define SSD1306_SETDISPLAYOFFSET 0xD3
/** Sets COM signals pin configuration to match the OLED panel layout. */
#define SSD1306_SETCOMPINS 0xDA
/** This command adjusts the VCOMH regulator output. */
#define SSD1306_SETVCOMDETECT 0xDB
/** Set Display Clock Divide Ratio/ Oscillator Frequency. */
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
/** Set Pre-charge Period */
#define SSD1306_SETPRECHARGE 0xD9
/** Deactivate scroll */
#define SSD1306_DEACTIVATE_SCROLL 0x2E
/** No Operation Command. */
#define SSD1306_NOP 0XE3


bool ssd1306_command(uint8_t c);
void ssd1306_set_row(uint8_t row);
void ssd1306_set_col(uint8_t col);
void ssd1306_clear();
bool ssd1306_init(uint8_t i2c_num, uint8_t i2c_address, ssd1306_type_t type);
void ssd1306_deinit();
void ssd1306_sleep();
void ssd1306_contrast(uint8_t contrast);
bool ssd1306_printf(const char *format, ...);
