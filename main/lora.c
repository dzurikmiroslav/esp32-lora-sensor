#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "mbedtls/aes.h"
#include "lmic.h"
#include "nvs.h"

#include "lora.h"
#include "lmic_hal.h"
#include "storage_key.h"

static const char *TAG = "lora";

static TaskHandle_t runloop_task;

static SemaphoreHandle_t send_semhr;

QueueHandle_t lora_join_queue;

void os_getArtEui(u1_t *buf)
{
    size_t length = sizeof(uint8_t) * 8;
    nvs_get_blob(storage, STORAGE_KEY_APP_EUI, (void*) buf, &length);
}

void os_getDevEui(u1_t *buf)
{
    size_t length = sizeof(uint8_t) * 8;
    nvs_get_blob(storage, STORAGE_KEY_DEV_EUI, (void*) buf, &length);
}

void os_getDevKey(u1_t *buf)
{
    size_t length = sizeof(uint8_t) * 16;
    nvs_get_blob(storage, STORAGE_KEY_DEV_KEY, (void*) buf, &length);
}

void onEvent(ev_t ev)
{
    bool join_status;
    ESP_LOGI(TAG, "Lora event %d", ev);
    switch (ev) {
        case EV_JOINED:
            LMIC_setLinkCheckMode(0);
            //LMIC.dn2Dr = DR_SF9;

            join_status = true;
            xQueueSend(lora_join_queue, &join_status, portMAX_DELAY);
            break;
        case EV_JOIN_FAILED:
            join_status = false;
            xQueueSend(lora_join_queue, &join_status, portMAX_DELAY);
            break;
        case EV_TXCOMPLETE:
            if (LMIC.txrxFlags & TXRX_ACK) {
                ESP_LOGD(TAG, "Received ack");
            }
            if (LMIC.dataLen) {
                ESP_LOGD(TAG, "Received payload");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, LMIC.frame, LMIC.dataLen, ESP_LOG_DEBUG);
            }
            xSemaphoreGive(send_semhr);
            break;
        default:
            break;
    }
}

void lmic_aes_encrypt(u1_t *data, u1_t *key)
{
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, data, data);
    mbedtls_aes_free(&ctx);
}

static void runloop_func(void *param)
{
    os_runloop();
}

void lora_init(spi_host_device_t spi_host, uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1)
{
    lmic_pins.spi_host = spi_host;
    lmic_pins.nss = nss;
    lmic_pins.rxtx = rxtx;
    lmic_pins.rst = rst;
    lmic_pins.dio0 = dio0;
    lmic_pins.dio1 = dio1;

    lora_join_queue = xQueueCreate(1, sizeof(bool));
    send_semhr = xSemaphoreCreateBinary();

    os_init();

    LMIC_reset();
    LMIC_setDrTxpow(DR_SF7, 14);

    xTaskCreate(runloop_func, "runloop_task", 4 * 1024, NULL, 10, &runloop_task);
}

void lora_deinit()
{
    lmic_hal_enter_critical();
    LMIC_shutdown();
    vTaskSuspend(runloop_task);
    lmic_hal_leave_critical();

    lmic_hal_free();

    vQueueDelete(lora_join_queue);
    vSemaphoreDelete(send_semhr);
}

void lora_start_joining()
{
    lmic_hal_enter_critical();

    LMIC_reset();
    LMIC_startJoining();

    lmic_hal_wakeup();
    lmic_hal_leave_critical();
}

void lora_get_counters(uint32_t *seqno_up, uint32_t *seqno_dw)
{
    *seqno_up = LMIC.seqnoUp;
    *seqno_dw = LMIC.seqnoDn;
}

void lora_get_session(uint32_t *dev_addr, uint8_t *nwk_key, uint8_t *art_key)
{
    *dev_addr = LMIC.devaddr;
    memcpy(nwk_key, LMIC.nwkKey, sizeof(uint8_t) * 16);
    memcpy(art_key, LMIC.artKey, sizeof(uint8_t) * 16);
}

void lora_set_session(uint32_t dev_addr, uint8_t *nwk_key, uint8_t *art_key, uint32_t seqno_up, uint32_t seqno_dw)
{
    lmic_hal_enter_critical();

    LMIC_setSession(0x1, dev_addr, nwk_key, art_key);
    LMIC_setLinkCheckMode(0);
    LMIC.dn2Dr = DR_SF9;
    LMIC_setDrTxpow(DR_SF7, 14);

    LMIC.seqnoUp = seqno_up;
    LMIC.seqnoDn = seqno_dw;

    lmic_hal_wakeup();
    lmic_hal_leave_critical();
}

void lora_send(const uint8_t *payload, size_t length)
{
    uint8_t confirmed = 0;
    nvs_get_u8(storage, STORAGE_KEY_CONFM, &confirmed);

    lmic_hal_enter_critical();

    LMIC_setTxData2(1, payload, length, confirmed);

    lmic_hal_wakeup();
    lmic_hal_leave_critical();

    xSemaphoreTake(send_semhr, portMAX_DELAY);
}
