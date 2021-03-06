#pragma once
#include "cJSON.h"
#include "nvs_flash.h"

class ConfigProvider
{
private:
    cJSON *root;
    nvs_handle openNVS(const char *ns);
    void closeNVS(nvs_handle handle);

public:
    ConfigProvider();

    bool  loadJSON(const char *buffer);
    char *saveJSON();
    bool  loadFile(const char *file);
    bool  saveFile(const char *file, bool update_only=false);
    bool  loadNVS(const char *ns);
    bool  saveNVS(const char *ns, bool update_only=false);

    char* getString(const char *key, char *default_value);
    bool  getString(const char *key, char *default_value, char *out);
    void  setString(const char *key, char *value);

    int   getInteger(const char *key, int default_value);
    bool  getInteger(const char *key, int default_value, int *out);
    void  setInteger(const char *key, int value);

    double getDouble(const char *key, double default_value);
    bool   getDouble(const char *key, double default_value, double *out);
    void   setDouble(const char *key, double value);
};
