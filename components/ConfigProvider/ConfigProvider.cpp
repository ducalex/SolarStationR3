#include "nvs_flash.h"
#include "esp_log.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "cJSON.h"
#include "ConfigProvider.h"

static const char *MODULE = "Config";
#define _debug ESP_ERROR_CHECK_WITHOUT_ABORT

ConfigProvider::ConfigProvider()
{
    root = cJSON_CreateObject();
}

bool ConfigProvider::loadJSON(const char *buffer)
{
    cJSON *new_root = cJSON_Parse(buffer);

    if (new_root == NULL) {
        ESP_LOGW(MODULE, "Configuration failed to load from string");
        if (root == NULL) root = cJSON_CreateObject();
        return false;
    }

    cJSON_free(root);
    root = new_root;

    ESP_LOGI(MODULE, "Configuration loaded from string, %d entries found", cJSON_GetArraySize(root));
    return true;
}

char *ConfigProvider::saveJSON()
{
    return cJSON_Print(root);
}

bool ConfigProvider::loadFile(const char *file)
{
    FILE *fp = fopen(file, "rb");

    if (fp == NULL) {
        ESP_LOGW(MODULE, "Unable to open file: %s", file);
        return false;
    }

    char *buffer = (char*)malloc(8 * 1024);
    fread(buffer, 1, 8 * 1024, fp);

    cJSON *new_root = cJSON_Parse(buffer);

    free(buffer);
    fclose(fp);

    if (new_root == NULL) {
        ESP_LOGW(MODULE, "Configuration failed to load from %s", file);
        if (root == NULL) root = cJSON_CreateObject();
        return false;
    }

    cJSON_free(root);
    root = new_root;

    ESP_LOGI(MODULE, "Configuration loaded from %s, %d entries found", file, cJSON_GetArraySize(root));
    return true;
}

bool ConfigProvider::saveFile(const char *file, bool update_only)
{
    char *data = cJSON_Print(root);

    if (data == NULL) {
        return false;
    }

    if (update_only) {
        FILE *fp = fopen(file, "rb");
        if (fp != NULL) {
            char *buffer = (char*)malloc(8 * 1024);
            fread(buffer, 1, 8 * 1024, fp);
            fclose(fp);

            if (strncmp(buffer, data, strlen(data)) == 0) {
                ESP_LOGI(MODULE, "File content identical, no need to update");
                free(buffer);
                return false;
            }
            free(buffer);
        }
    }

    FILE *fp = fopen(file, "wb");

    if (fp == NULL) {
        ESP_LOGW(MODULE, "Unable to open file: %s", file);
        free(data);
        return false;
    }

    bool ret = (fwrite(data, strlen(data), 1, fp) == 1);

    fclose(fp);
    free(data);

    ESP_LOGI(MODULE, "Configuration saved to %s: %d", file, ret);
    return ret;
}

bool ConfigProvider::loadNVS(const char *ns)
{
    size_t length = 4000;
    char  *buffer = (char *)malloc(length);

    cJSON *new_root = NULL;
    nvs_handle nvs_h = openNVS(ns);

    if (nvs_get_str(nvs_h, "json", buffer, &length) == ESP_OK) {
        new_root = cJSON_Parse(buffer);
    }
    closeNVS(nvs_h);
    free(buffer);

    if (new_root == NULL) {
        ESP_LOGW(MODULE, "Configuration failed to load from NVS");
        if (root == NULL) root = cJSON_CreateObject();
        return false;
    }

    cJSON_free(root);
    root = new_root;

    ESP_LOGI(MODULE, "Configuration loaded from NVS, %d entries found", cJSON_GetArraySize(root));
    return true;
}

bool ConfigProvider::saveNVS(const char *ns, bool update_only)
{
    char *data = cJSON_PrintUnformatted(root);

    esp_err_t ret = ESP_OK;

    nvs_handle nvs_h = openNVS(ns);

    if (data == NULL) {
        ret = nvs_erase_key(nvs_h, "json");
        ESP_LOGI(MODULE, "NVS content deleted (config empty)");
    }
    else {
        bool update = true;

        if (update_only) {
            size_t length = 4000;
            char *buffer = (char*)malloc(4000);
            if (nvs_get_str(nvs_h, "json", buffer, &length) == ESP_OK && strncmp(buffer, data, strlen(data)) == 0) {
                update = false;
            }
            free(buffer);
        }

        if (update) {
            ret = nvs_set_str(nvs_h, "json", data);
            ESP_LOGI(MODULE, "NVS content written, result: %s", esp_err_to_name(ret));
        } else {
            ESP_LOGI(MODULE, "NVS content identical, no need to update");
        }
        free(data);
    }

    closeNVS(nvs_h);

    return (ret == ESP_OK);
}

nvs_handle ConfigProvider::openNVS(const char *ns)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        _debug( nvs_flash_erase() );
        _debug( nvs_flash_init() );
    }

    nvs_handle nvs_h;
    _debug( nvs_open(ns, NVS_READWRITE, &nvs_h) );

    return nvs_h;
}

void ConfigProvider::closeNVS(nvs_handle nvs_h)
{
    _debug( nvs_commit(nvs_h) );
    nvs_close(nvs_h);
}


char* ConfigProvider::getString(const char *key, char *default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL || obj->valuestring == NULL) {
        return default_value;
    }
    return obj->valuestring;
}

bool ConfigProvider::getString(const char *key, char *default_value, char *out)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL || obj->valuestring == NULL) {
        if (out != default_value) {
            strcpy(out, default_value);
        }
        return false;
    }
    strcpy(out, obj->valuestring);
    return true;
}

void ConfigProvider::setString(const char *key, char *value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        cJSON_AddStringToObject(root, key, value);
    } else {
        obj = (value == NULL) ? cJSON_CreateNull() : cJSON_CreateString(value);
        cJSON_ReplaceItemInObject(root, key, obj);
    }
}


int ConfigProvider::getInteger(const char *key, int default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        return default_value;
    }
    return obj->valueint;
}

bool ConfigProvider::getInteger(const char *key, int default_value, int *out)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        *out = default_value;
        return false;
    }
    *out = obj->valueint;
    return true;
}

void ConfigProvider::setInteger(const char *key, int value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        cJSON_AddNumberToObject(root, key, value);
    } else {
        cJSON_SetNumberValue(obj, value);
    }
}


double ConfigProvider::getDouble(const char *key, double default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        return default_value;
    }
    return obj->valuedouble;
}

bool ConfigProvider::getDouble(const char *key, double default_value, double *out)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        *out = default_value;
        return false;
    }
    *out = obj->valuedouble;
    return true;
}

void ConfigProvider::setDouble(const char *key, double value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        cJSON_AddNumberToObject(root, key, value);
    } else {
        cJSON_SetNumberValue(obj, value);
    }
}
