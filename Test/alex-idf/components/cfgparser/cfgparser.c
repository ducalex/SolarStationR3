// Simple configuration file handler

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <cJSON.h>

static cJSON *root;


bool config_load_file(const char *file)
{
    char *buffer = malloc(16 * 1024);

    FILE *fp = fopen(file, "r");

    if (fp == NULL)
        return false;

    fread(buffer, 1, 16 * 1024, fp);

    root = cJSON_Parse(buffer);

    fclose(fp);
    free(buffer);

    return (root != NULL);
}


bool config_save_file(const char *file)
{
    return false;
}


void config_free()
{
    cJSON_Delete(root);
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


#if 0
// Poor hand crafted parser, it just can't compete with json :(
typedef struct {
    char *key;
    char *value;
} config_param_t;

static config_param_t params[64];
static int params_count;


static char *trim(char *s)
{
    int l = strlen(s);

    while(isspace((unsigned char)s[l - 1])) --l;
    while(*s && isspace((unsigned char)*s)) ++s, --l;

    return strndup(s, l);
}


bool config_load_file(const char *file)
{
    char buffer[512];
    char *ptr;

    FILE *fp = fopen(file, "r");

    if (fp == NULL)
        return false;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (buffer[0] == '\r' || buffer[0] == '\n' || buffer[0] == '#')
            continue;

        if ((ptr = strtok(buffer, "=")) != NULL) {
            config_param_t *param = &params[params_count++];

            param->key = trim(ptr);
            ptr = strtok(NULL, "=");
            param->value = trim(ptr);
        }
    }

    fclose(fp);

    return true;
}


void config_free()
{

}


const char *config_get_string(const char *key)
{
    for (int i = 0; i < params_count; i++) {
        if (strcasecmp(key, params[i].key) == 0) {
            return params[i].value;
        }
    }
    return NULL;
}


const int config_get_int(const char *key)
{
    for (int i = 0; i < params_count; i++) {
        if (strcasecmp(key, params[i].key) == 0) {
            return atoi(params[i].value);
        }
    }
    return NULL;
}
#endif