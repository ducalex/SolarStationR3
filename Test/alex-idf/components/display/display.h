typedef struct {

} display_provider_t;

void display_init();
void display_sleep();
void display_deinit();
void display_set_row(int row);
void display_set_col(int col);
void display_printf(char *format, ...);
void display_clear();
