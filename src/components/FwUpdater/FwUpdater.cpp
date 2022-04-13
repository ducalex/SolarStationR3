#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include "FwUpdater.h"

static const char *MODULE = "FwUpdater";
static const char *ERROR_STRINGS[] = {
    "OK",
    "ERR_NO_FREE_OTA_PARTITION",
    "ERR_OTA_PARTITION_IN_USE",
    "ERR_INVALID_MAGIC_BYTE",
    "ERR_PENDING_VERIFY",
    "ERR_WRITE_FAILED",
    "ERR_SET_BOOT_FAILED",
    "ERR_CHECKSUM_FAILED",
    "ERR_INVALID_STATE",
    "ERR_UNSPECIFIED",
    "ERR_READ_ERROR",
    "ERR_USER_ABORT",
    "UNKNOWN"
};

bool FwUpdaterClass::begin(size_t size)
{
    if (m_inProgress && m_lastError == FWU_OK) {
        ESP_LOGW(MODULE, "Update in progress, please call end() or reset() before calling begin() again");
        m_lastError = FWU_ERR_INVALID_STATE;
        return false;
    } else {
        m_reset();
    }

    const esp_partition_t *target = esp_ota_get_next_update_partition(NULL);
    const esp_partition_t *current = esp_ota_get_running_partition();
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

    if (target == NULL) {
        if (factory != NULL && current != factory) {
            ESP_LOGW(MODULE, "Single OTA partition in use. Rebooting to factory image to flash from there.");
            m_lastError = FWU_ERR_OTA_PARTITION_IN_USE;
            return false;
        }
        ESP_LOGE(MODULE, "No free OTA partition. Cannot upgrade.");
        m_lastError = FWU_ERR_NO_FREE_OTA_PARTITION;
        return false;
    }

    esp_err_t err = esp_ota_begin(target, size, &m_otaHandle);

    if (err != ESP_OK) {
        m_lastError = ESP_ERR_OTA_ROLLBACK_INVALID_STATE ? FWU_ERR_PENDING_VERIFY : FWU_ERR_UNSPECIFIED;
        ESP_LOGE(MODULE, "OTA begin error: %s (ESP error: %s)", getErrorStr(), esp_err_to_name(err));
        return false;
    } else {
        m_inProgress = true;
        m_bytesTotal = size > 0 ? size : target->size;
        m_partition = target;
        ESP_LOGI(MODULE, "Flashing firmware to '%s' at address 0x%x...", target->label, target->address);
        return true;
    }
}

void FwUpdaterClass::m_reset()
{
    m_partition = NULL;
    m_bytesTotal = 0;
    m_bytesWritten = 0;
    m_lastError = FWU_OK;
    m_inProgress = false;
    // m_onProgress_cb = NULL;
    // m_onVersionCheck_cb = NULL;
}

bool FwUpdaterClass::end()
{
    if (!m_inProgress || m_lastError != FWU_OK) {
        return false;
    }

    esp_err_t err = esp_ota_end(m_otaHandle);
    if (err != ESP_OK) {
        m_lastError =  (err == ESP_ERR_OTA_VALIDATE_FAILED) ? FWU_ERR_CHECKSUM_FAILED : FWU_ERR_UNSPECIFIED;
        ESP_LOGE(MODULE, "OTA End failed (ESP error: %s)", esp_err_to_name(err));
        return false;
    }

    if (esp_ota_set_boot_partition(m_partition) != ESP_OK) {
        m_lastError = FWU_ERR_SET_BOOT_FAILED;
        ESP_LOGE(MODULE, "Set Boot Partition failed");
        return false;
    }

    ESP_LOGI(MODULE, "Firmware successfully flashed!");
    m_reset();

    return true;
}

void FwUpdaterClass::onProgress(FwuProgress callback)
{
    m_onProgress_cb = callback;
}

void FwUpdaterClass::onValidateHeader(FwuValidateHeader callback)
{
    m_onValidateHeader_cb = callback;
}

bool FwUpdaterClass::write(uint8_t *data, size_t size)
{
    if (!m_inProgress || m_lastError != FWU_OK) {
        return false;
    }

    if (m_bytesWritten == 0 && size > 0 && data[0] != ESP_IMAGE_HEADER_MAGIC) {
        ESP_LOGE(MODULE, "OTA image has invalid magic byte");
        m_lastError = FWU_ERR_INVALID_MAGIC_BYTE;
        return false;
    }
    if (m_bytesWritten == 0 && size >= sizeof(esp_app_desc_t) + 32) {
        memcpy(&m_newAppDescription, data + 32, sizeof(m_newAppDescription));
        ESP_LOGI(MODULE, "New image information: ");
        ESP_LOGI(MODULE, "  Project name: %s", m_newAppDescription.project_name);
        ESP_LOGI(MODULE, "  Version: %s", m_newAppDescription.version);
        ESP_LOGI(MODULE, "  Compile date: %s %s", m_newAppDescription.date, m_newAppDescription.time);

        if (m_onValidateHeader_cb && !m_onValidateHeader_cb(esp_ota_get_app_description(), &m_newAppDescription)) {
            m_lastError = FWU_ERR_USER_ABORT;
            return false;
        }
    }

    if (esp_ota_write(m_otaHandle, data, size) == ESP_OK) {
        m_bytesWritten += size;
        if (m_onProgress_cb) {
            m_onProgress_cb(m_bytesWritten, m_bytesTotal);
        }
    } else {
        m_lastError = FWU_ERR_WRITE_FAILED;
        ESP_LOGE(MODULE, "Flash write failed (%d bytes at pos %d)", size, m_bytesWritten);
    }

    return (m_lastError == FWU_OK);
}

bool FwUpdaterClass::writeFromHTTP(const char *url, int timeout, const char *username, const char *password)
{
    if (!m_inProgress || m_lastError != FWU_OK) {
        return false;
    }

    ESP_LOGI(MODULE, "Reading firmware from '%s'...", url);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.username = username;
    config.password = password;
    config.timeout_ms = timeout;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (esp_http_client_perform(client) == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        int length = esp_http_client_get_content_length(client);

        ESP_LOGI(MODULE, "HTTP status = %d, content_length = %d", code, length);

        if (code == 200) {
            uint8_t *buffer = (uint8_t*)malloc(16*1024);
            int len = 0;
            while ((len = esp_http_client_read(client, (char*)buffer, 16*1024)) > 0) {
                if (!write(buffer, len)) {
                    break;
                }
            }
            if (len == -1) m_lastError = FWU_ERR_READ_ERROR;
            free(buffer);
        } else {
            m_lastError = FWU_ERR_READ_ERROR;
        }
    } else {
        m_lastError = FWU_ERR_READ_ERROR;
        ESP_LOGE(MODULE, "HTTP request failed");
    }

    esp_http_client_cleanup(client);

    return (m_lastError == FWU_OK);
}

bool FwUpdaterClass::writeFromFile(const char *filePath)
{
    if (!m_inProgress || m_lastError != FWU_OK) {
        return false;
    }

    ESP_LOGI(MODULE, "Reading firmware from '%s'...", filePath);

    FILE *file = fopen(filePath, "r");
    if (!file) {
        m_lastError = FWU_ERR_READ_ERROR;
        ESP_LOGE(MODULE, "Unable to open file!");
        return false;
    }

    uint8_t *buffer = (uint8_t*)malloc(16*1024);
    int len = 0;
    while ((len = fread(buffer, 1, 16*1024, file)) > 0) {
        if (!write(buffer, len)) {
            break;
        }
    }
    if (ferror(file) != 0) {
        m_lastError = FWU_ERR_READ_ERROR;
    }
    free(buffer);
    fclose(file);

    return (m_lastError == FWU_OK);
}

void FwUpdaterClass::rollback()
{
    if (esp_ota_check_rollback_is_possible()) {
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}

void FwUpdaterClass::markAppValid()
{
    if (esp_ota_check_rollback_is_possible()) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
            ESP_LOGI(MODULE, "The running app has been marked as valid!");
            esp_ota_erase_last_boot_app_partition();
        }
    }
}

void FwUpdaterClass::handlePendingTasks()
{

}

void FwUpdaterClass::rebootToFactory()
{
    const esp_partition_t *factory = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);

    if (esp_ota_set_boot_partition(factory) == ESP_OK) {
        esp_restart();
    }
}

const char *FwUpdaterClass::getErrorStr()
{
    if (m_lastError < FWU_ERR_MAX) {
        return ERROR_STRINGS[m_lastError];
    } else {
        return ERROR_STRINGS[FWU_ERR_MAX];
    }
}


FwUpdaterClass FwUpdater;