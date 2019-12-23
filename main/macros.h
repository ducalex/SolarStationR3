// Macros to get values
#define CFG_STR(key) config.getString(key, (char*)"")
#define CFG_INT(key) config.getInteger(key, 0)
#define CFG_DBL(key) config.getDouble(key, 0)
// Macros to set values
#define CFG_SET_STR(key, value) config.setString(key, (char*)value);
#define CFG_SET_INT(key, value) config.setInteger(key, value)
#define CFG_SET_DBL(key, value) config.setDouble(key, value)
// Macros to load default value if they key doesn't exist
#define CFG_LOAD_STR(key, _default) config.setString(key, config.getString(key, (char*)_default));
#define CFG_LOAD_INT(key, _default) config.setInteger(key, config.getInteger(key, _default))
#define CFG_LOAD_DBL(key, _default) config.setDouble(key, config.getDouble(key, _default))

#define POWER_SAVE_INTERVAL(in, th, vb) (((float)th <= vb || vb < 2) ? in : (uint)ceil(((th-vb) * 10.00) * in))
#define PRINT_MEMORY_STATS() { \
  multi_heap_info_t info; \
  heap_caps_get_info(&info, MALLOC_CAP_DEFAULT); \
  ESP_LOGI("Memory", "Used: %d KB   Free: %d KB", \
        info.total_allocated_bytes / 1024, info.total_free_bytes / 1024); }

// Lossless float to double conversion. In some cases it will overflow and die of course.
#define F2D(n) ((double)((long)((n) * 100000)) / 100000)
// Signed integer to double
#define I2D(n) ((double)(long)(n))

#define ARRAY_FILL(array, start, count, value) {for (int __i = 0; __i < (count); __i++) array[(start) + __i] = (value);}
