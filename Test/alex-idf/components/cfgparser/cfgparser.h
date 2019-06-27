bool config_load_file(const char *file);
bool config_save_file(const char *file);
void config_free();
char *config_get_string(const char *key, char *default_value);
int config_get_int(const char *key, int default_value);
double config_get_double(const char *key, double default_value);
