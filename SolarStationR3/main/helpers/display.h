#include "SSD1306AsciiWire.h"
#include "../config.h"

class DisplaySerial
{
public:
    void begin() {}
    void end() {}
    void clear(){}
    void printf(const char *format, ...)
    {
        va_list argptr;
        va_start(argptr, format);
        vprintf(format, argptr);
        va_end(argptr);
    }
};

class DisplayOLED
{
private:
    SSD1306AsciiWire m_display;
public:
    void begin()
    {
        m_display.begin(&Adafruit128x64, 0x3C);
        m_display.setFont(System5x7);
        m_display.setScrollMode(SCROLL_MODE_AUTO);
    }

    void end()
    {
        m_display.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    }

    void clear()
    {
        m_display.clear();
    }

    void printf(const char *format, ...)
    {
        char buffer[512];
        va_list argptr;
        va_start(argptr, format);
        vsprintf(buffer, format, argptr);
        va_end(argptr);
        m_display.print(buffer);
    }
};


#if USE_OLED
static DisplayOLED Display;
#else
static DisplaySerial Display;
#endif
