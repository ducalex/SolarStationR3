#include "SSD1306AsciiWire.h"

static class {
    SSD1306AsciiWire m_display;
    bool m_useOLED = false;
  public:
    void begin()
    {
        Wire.beginTransmission(OLED_I2C_ADDRESS);
        m_useOLED = (Wire.endTransmission() == 0);

        if (m_useOLED) {
            m_display.begin(&Adafruit128x64, OLED_I2C_ADDRESS);
            m_display.setFont(System5x7);
            m_display.setScrollMode(SCROLL_MODE_AUTO);
        }
    }

    void end()
    {
        if (m_useOLED) m_display.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    }

    void clear()
    {
        if (m_useOLED) m_display.clear();
    }

    void printf(const char *format, ...)
    {
        char buffer[512];
        va_list argptr;
        va_start(argptr, format);
        vsprintf(buffer, format, argptr);
        va_end(argptr);

        if (m_useOLED) {
            m_display.print(buffer);
        } else {
            ::printf("[DISPLAY] %s", buffer);
        }
    }

    bool isPresent()
    {
        return m_useOLED;
    }
} Display;
