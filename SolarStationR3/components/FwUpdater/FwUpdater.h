#pragma once
#include "esp_ota_ops.h"
#include "esp_system.h"

typedef void (*FwProgress)(size_t done, size_t total);

typedef enum {
    /* Everything is OK */
    FWU_OK,
    /* There is no (free) OTA partition */
    FWU_ERR_NO_FREE_OTA_PARTITION,
    /* There is only one OTA partition and we're on it */
    FWU_ERR_OTA_PARTITION_IN_USE,
    /* Magic byte invalid */
    FWU_ERR_INVALID_MAGIC_BYTE,
    /* Running app hasn't been marked as valid yet (markAppValid()) */
    FWU_ERR_PENDING_VERIFY,
    /* Unable to write to flash */
    FWU_ERR_WRITE_FAILED,
    /* Unable to change boot partition */
    FWU_ERR_SET_BOOT_FAILED,
    /* The image's checksum doesn't match */
    FWU_ERR_CHECKSUM_FAILED,
    /* FwUpdater is in an invalid state */
    FWU_ERR_INVALID_STATE,
    /* Unspecified error */
    FWU_ERR_UNSPECIFIED,
    /* Read error occured from an external source (file/http) */
    FWU_ERR_READ_ERROR,
    /* N/A */
    FWU_ERR_MAX,
} fwu_error_t;

class FwUpdaterClass
{
protected:
    const esp_partition_t *m_partition;
    esp_ota_handle_t m_otaHandle;
    esp_app_desc_t m_newAppDescription;
    FwProgress m_onProgress_cb;
    bool m_inProgress;
    size_t m_bytesWritten;
    size_t m_bytesTotal;
    fwu_error_t m_lastError;
    void m_reset();

public:
    FwUpdaterClass() { m_reset(); }

    /**
     * Begin the OTA process
     */
    bool begin(size_t size = 0);

    /**
     * End the OTA process
     */
    bool end();

    /**
     * Reset the FwUpdater / Cancel any running process
     */
    void cancel() {m_reset();}

    /**
     * Get the last error to occur
     */
    fwu_error_t getError() { return m_lastError; }

    /**
     * Get the last error to occur in string form
     */
    const char *getErrorStr();

    /**
     * Function to be called on each successful write() call
     */
    void onProgress(FwProgress callback);

    /**
     * Write data to flash, the position is automatically advanced
     */
    bool write(uint8_t *data, size_t size);

    /**
     * Read data from an HTTP URL and write it to flash
     */
    bool writeFromHTTP(const char *url, int timeout = 30000, const char *username = nullptr, const char *password = nullptr);

    /**
     * Read data from a file and write it to flash
     */
    bool writeFromFile(const char *filePath);

    /**
     * Rollback and reboot
     */
    void rollback();

    /**
     * Mark the app as valid, otherwise the previous app will boot next time
     */
    void markAppValid();

    /**
     * This should be called in your entry function. It handles pending tasks
     */
    void handlePendingTasks();

    /**
     * Change boot partition to factory and reboot, if possible
     */
    void rebootToFactory();
};

extern FwUpdaterClass FwUpdater;