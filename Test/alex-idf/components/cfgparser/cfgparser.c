static const char *MODULE = "config";

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

#include "esp_log.h"
#include "nvs_flash.h"


static cJSON *root;


bool config_load_file(const char *file)
{
    char *buffer = malloc(16 * 1024);

    FILE *fp = fopen(file, "r");

    if (fp == NULL) {
        ESP_LOGW(MODULE, "Unable to open file: %s", file);
        return false;
    }


    fread(buffer, 1, 16 * 1024, fp);

    root = cJSON_Parse(buffer);

    fclose(fp);
    free(buffer);

    if (root != NULL) {
        ESP_LOGI(MODULE, "Configuration loaded from %s", file);
    } else {
        ESP_LOGW(MODULE, "Configuration failed to load from %s", file);
    }

    return (root != NULL);
}


bool config_save_file(const char *file)
{
    char *data = NULL;
    bool ret = false;

    FILE *fp = fopen(file, "r");

    if (fp == NULL) {
        ESP_LOGW(MODULE, "Unable to open file: %s", file);
        return false;
    }

    if (root) {
        data = cJSON_Print(root);
    }

    if (data == NULL) {
        data = strdup("{\"Error\":\"No configuration\"}"); // Or return false?
    }

    ret = (fwrite(data, strlen(data), 1, fp) == 1);

    fclose(fp);
    free(data);

    return ret;
}


// At the moment we are lazy and just store the json in nvs, rather than mapping each K/V...
bool config_load_nvs(const char *namespace)
{
    char *buffer = malloc(4096);
    size_t len;
    nvs_handle nvs_h;
    nvs_open(namespace, NVS_READONLY, &nvs_h);
    if (nvs_get_str(nvs_h, "json", buffer, &len) == ESP_OK) {
        root = cJSON_Parse(buffer);
    }
    nvs_close(nvs_h);
    free(buffer);

    if (root != NULL) {
        ESP_LOGI(MODULE, "Configuration loaded from NVS");
    } else {
        ESP_LOGW(MODULE, "Configuration failed to load from NVS");
    }

    return (root != NULL);
}


bool config_save_nvs(const char *namespace)
{
    char *buffer = malloc(4096);
    size_t len;

    char *data = NULL;
    bool ret = false;

    if (root) {
        data = cJSON_PrintUnformatted(root);
    }

    if (data == NULL) {
        data = strdup("{\"Error\":\"No configuration\"}"); // Or return false?
    }

    nvs_handle nvs_h;
    nvs_open(namespace, NVS_READWRITE, &nvs_h);

    // To avoid flash wear we check if the currently stored value is identical to our "new" one
    if (nvs_get_str(nvs_h, "json", buffer, &len) != ESP_OK || strncmp(data, buffer, strlen(data)) != 0) {
        ret = nvs_set_str(nvs_h, "json", data);
        ESP_LOGI(MODULE, "NVS content written, result: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(MODULE, "NVS content identical, no need to overwrite");
    }

    nvs_commit(nvs_h);
    nvs_close(nvs_h);

    free(buffer);
    free(data);

    return (ret == ESP_OK);
}


void config_free()
{
    cJSON_Delete(root);
}


bool config_get_string_r(const char *key, char *out, int max_len)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL || obj->valuestring == NULL) return false;
    strncpy(out, obj->valuestring, max_len);
    return true;
}


bool config_get_int_r(const char *key, int *out)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj != NULL) *out = obj->valueint;
    return obj != NULL;
}


bool config_get_double_r(const char *key, double *out)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj != NULL) *out = obj->valuedouble;
    return obj != NULL;
}


char *config_get_string(const char *key, char *default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL || obj->valuestring == NULL) {
        return default_value;
    }
    return obj->valuestring;
}


int config_get_int(const char *key, int default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        return default_value;
    }
    return obj->valueint;
}


double config_get_double(const char *key, double default_value)
{
    cJSON *obj = cJSON_GetObjectItem(root, key);
    if (obj == NULL) {
        return default_value;
    }
    return obj->valuedouble;
}
