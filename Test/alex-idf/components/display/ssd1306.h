// Mostly stolen from SSD1306Ascii
#include <stdint.h>

/** Newline will not scroll the display or RAM window. */
#define SCROLL_MODE_OFF  0
/** Newline will scroll both the display and RAM windows. */
#define SCROLL_MODE_AUTO 1
/** Newline scrolls the RAM window. The app scrolls the display window. */
#define SCROLL_MODE_APP  2

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
/** Set Pump voltage value: (30H~33H) 6.4, 7.4, 8.0 (POR), 9.0. */
#define SH1106_SET_PUMP_VOLTAGE 0X30
/** First byte of set charge pump mode */
#define SH1106_SET_PUMP_MODE 0XAD
/** Second byte charge pump on. */
#define SH1106_PUMP_ON 0X8B
/** Second byte charge pump off. */
#define SH1106_PUMP_OFF 0X8A


#define SSD1306_MODE_CMD     0X00
#define SSD1306_MODE_RAM     0X40


//------------------------------------------------------------------------------
/**
 * @struct DevType
 * @brief Device initialization structure.
 */
typedef struct  {
  /**
   * Pointer to initialization command bytes.
   */
  const uint8_t* initcmds;
  /**
   * Number of initialization bytes.
   */
  const uint8_t initSize;
  /**
   * Width of the diaplay in pixels.
   */
  const uint8_t lcdWidth;
  /**
   * Height of the display in pixels.
   */
  const uint8_t lcdHeight;
} DevType;


//------------------------------------------------------------------------------
// this section is based on https://github.com/adafruit/Adafruit_SSD1306
/** Initialization commands for a 128x32 SSD1306 oled display. */
static const uint8_t Adafruit128x32init[] = {
    // Init sequence for Adafruit 128x32 OLED module
    SSD1306_DISPLAYOFF,
    SSD1306_SETDISPLAYCLOCKDIV, 0x80,  // the suggested ratio 0x80
    SSD1306_SETMULTIPLEX, 0x1F,        // ratio 32
    SSD1306_SETDISPLAYOFFSET, 0x0,     // no offset
    SSD1306_SETSTARTLINE | 0x0,        // line #0
    SSD1306_CHARGEPUMP, 0x14,          // internal vcc
    SSD1306_MEMORYMODE, 0x02,          // page mode
    SSD1306_SEGREMAP | 0x1,            // column 127 mapped to SEG0
    SSD1306_COMSCANDEC,                // column scan direction reversed
    SSD1306_SETCOMPINS, 0x02,          // sequential COM pins, disable remap
    SSD1306_SETCONTRAST, 0x7F,         // contrast level 127
    SSD1306_SETPRECHARGE, 0xF1,        // pre-charge period (1, 15)
    SSD1306_SETVCOMDETECT, 0x40,       // vcomh regulator level
    SSD1306_DISPLAYALLON_RESUME,
    SSD1306_NORMALDISPLAY,
    SSD1306_DISPLAYON
};
/** Initialize a 128x32 SSD1306 oled display. */
static const DevType Adafruit128x32 = {
  Adafruit128x32init,
  sizeof(Adafruit128x32init),
  128,
  32
};
//------------------------------------------------------------------------------
// This section is based on https://github.com/adafruit/Adafruit_SSD1306
/** Initialization commands for a 128x64 SSD1306 oled display. */
static const uint8_t Adafruit128x64init[] = {
    // Init sequence for Adafruit 128x64 OLED module
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
    SSD1306_SETCONTRAST, 0x7F,         // contrast level 127
    SSD1306_SETPRECHARGE, 0xF1,        // pre-charge period (1, 15)
    SSD1306_SETVCOMDETECT, 0x40,       // vcomh regulator level
    SSD1306_DISPLAYALLON_RESUME,
    SSD1306_NORMALDISPLAY,
    SSD1306_DISPLAYON
};
/** Initialize a 128x64 oled display. */
static const DevType Adafruit128x64 = {
  Adafruit128x64init,
  sizeof(Adafruit128x64init),
  128,
  64
};
