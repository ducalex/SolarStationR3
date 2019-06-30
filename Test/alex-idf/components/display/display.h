typedef struct {

} display_provider_t;
#if 1
void display_init();
void display_sleep();
void display_deinit();
void display_set_row(int row);
void display_set_col(int col);
void display_printf(char *format, ...);
void display_clear();

#else

#include "SSD1306.h"
#define display_init() ssd1306_init(0, SSD1306_I2C_ADDRESS, SSD1306_128x64)
#define display_sleep ssd1306_sleep
#define display_deinit ssd1306_deinit
#define display_set_row ssd1306_set_row
#define display_set_col ssd1306_set_col
#define display_printf ssd1306_printf
#define display_clear ssd1306_clear
#endif