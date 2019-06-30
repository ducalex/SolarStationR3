bool config_load_file(const char *file);
bool config_save_file(const char *file);
bool config_load_nvs(const char *namespace);
bool config_save_nvs(const char *namespace);

void config_free();

bool config_get_string_r(const char *key, char *out, int max_len);
bool config_get_int_r(const char *key, int *out);
bool config_get_double_r(const char *key, double *out);

char *config_get_string(const char *key, char *default_value);
int config_get_int(const char *key, int default_value);
double config_get_double(const char *key, double default_value);