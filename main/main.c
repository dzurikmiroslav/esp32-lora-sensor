#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "lora.h"
#include "storage_key.h"
#include "ble.h"
#include "i2c.h"
#include "bme.h"

#define RFM_SCLK    GPIO_NUM_18
#define RFM_MISO    GPIO_NUM_19
#define RFM_MOSI    GPIO_NUM_23
#define RFM_NSS     GPIO_NUM_21
#define RFM_RXTX    LORA_UNUSED_PIN
#define RFM_RESET   GPIO_NUM_22
#define RFM_DIO0    GPIO_NUM_5
#define RFM_DIO1    GPIO_NUM_4

#define BLE_BUTTON  GPIO_NUM_15
#define BLE_LED     GPIO_NUM_2
#define LORA_LED    GPIO_NUM_12
#define ERR_LED     GPIO_NUM_13

#define BLE_CONNECTION_TIMEOUT  30000  //30sec
#define LORA_JOIN_TIMEOUT       900000 //15min

static const char *TAG = "main";

static RTC_DATA_ATTR uint32_t dev_addr = 0;
static RTC_DATA_ATTR uint8_t nwk_key[16];
static RTC_DATA_ATTR uint8_t art_key[16];
static RTC_DATA_ATTR uint32_t seqno_up;
static RTC_DATA_ATTR uint32_t seqno_dw;
static RTC_DATA_ATTR time_t send_time = 0;

static esp_timer_handle_t send_timer = NULL;

static TaskHandle_t join_task;
static TaskHandle_t send_task;

static SemaphoreHandle_t join_task_done_sem;
static SemaphoreHandle_t ble_task_done_sem;
static SemaphoreHandle_t send_sem;
static SemaphoreHandle_t join_mutex;
static SemaphoreHandle_t send_mutex;

nvs_handle storage;

static void spi_init()
{
    spi_bus_config_t spi_cfg;
    memset(&spi_cfg, 0, sizeof(spi_bus_config_t));
    spi_cfg.miso_io_num = RFM_MISO;
    spi_cfg.mosi_io_num = RFM_MOSI;
    spi_cfg.sclk_io_num = RFM_SCLK;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &spi_cfg, 1));
}

static void blink_task_func(void *param)
{
    gpio_num_t led = (int) param;
    while (true) {
        gpio_set_level(led, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        gpio_set_level(led, 1);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static void arm_send_timer()
{
    uint16_t period;
    nvs_get_u16(storage, STORAGE_KEY_PERIOD, &period);

    struct timeval now;
    gettimeofday(&now, NULL);

    uint16_t dt = now.tv_sec - send_time;
    if (dt < period) {
        period -= dt;
    }

    ESP_LOGI(TAG, "Arm send timer for %ds", period);

    esp_timer_start_once(send_timer, period * 1000000);
}

static void join_task_func(void *param)
{
    esp_timer_stop(send_timer);
    send_time = 0;
    dev_addr = 0;

    size_t app_eui_len = 8;
    size_t dev_eui_len = 8;
    size_t dev_key_len = 16;
    if (nvs_get_blob(storage, STORAGE_KEY_APP_EUI, NULL, &app_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_DEV_EUI, NULL, &dev_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_DEV_KEY, NULL, &dev_key_len) == ESP_OK) {
        xSemaphoreTake(join_mutex, portMAX_DELAY);
        lora_start_joining();
        xSemaphoreGive(join_mutex);

        bool join_status;
        if (xQueueReceive(lora_join_queue, &join_status, LORA_JOIN_TIMEOUT / portTICK_PERIOD_MS) && join_status) {
            ESP_LOGI(TAG, "Login successful");
            lora_get_session(&dev_addr, nwk_key, art_key);
            xSemaphoreGive(send_sem);
        } else {
            ESP_LOGE(TAG, "Login not successful");
        }
    } else {
        ESP_LOGW(TAG, "Missing login credentials");
    }

    join_task = 0;
    xSemaphoreGive(join_task_done_sem);
    vTaskDelete(NULL);
}

static void ble_task_func(void *param)
{
    ble_event_t event;
    bool connection = false;
    bool receved;
    TaskHandle_t blink_task;

    ble_init();

    xTaskCreate(blink_task_func, "ble_blink_task", 1 * 1024, (void*)(int)BLE_LED, 5, &blink_task);

    bme_data_t data = bme_read();
    ble_set_telemetry(data.humidity, data.temperature, data.pressure);

    while ((receved = xQueueReceive(ble_event_queue, &event, BLE_CONNECTION_TIMEOUT / portTICK_PERIOD_MS)) || connection) {
        if (!receved) continue;
        switch (event) {
            case BLE_EVENT_CONNECT:
                connection = true;
                vTaskSuspend(blink_task);
                gpio_set_level(BLE_LED, 1);
                break;
            case BLE_EVENT_DISCONNECT:
                connection = false;
                vTaskResume(blink_task);
                break;
            case BLE_EVENT_PERIOD_UPDATE:
                if (dev_addr) {
                    esp_timer_stop(send_timer);
                    xSemaphoreGive(send_sem);
                }
                break;
            case BLE_EVENT_LORA_UPDATED:
                xSemaphoreTake(join_mutex, portMAX_DELAY);
                if (join_task) {
                    vTaskDelete(join_task);
                }
                xSemaphoreGive(join_mutex);
                xSemaphoreTake(join_task_done_sem, 0);
                xTaskCreate(join_task_func, "join_task", 4 * 1024, NULL, 10, &join_task);
                break;
            default:
                break;
        }
    }
    vTaskDelete(blink_task);

    ble_deinit();
    xSemaphoreGive(ble_task_done_sem);
    gpio_set_level(BLE_LED, 0);
    vTaskDelete(NULL);
}

static void IRAM_ATTR button_isr_handler(void *arg)
{
    if (xSemaphoreTakeFromISR(ble_task_done_sem, NULL)) {
        xTaskCreate(ble_task_func, "ble_task", 4 * 1024, NULL, 10, NULL);
    }
}

static void send_timer_callback(void *arg)
{
    xSemaphoreGive(send_sem);
}

static void measure_and_send(bool has_ble)
{
    bme_data_t data = bme_read();
    if (has_ble) {
        ble_set_telemetry(data.humidity, data.temperature, data.pressure);
    }
    lora_send(data.humidity, data.temperature, data.pressure, 100); //TODO battery

    struct timeval now;
    gettimeofday(&now, NULL);
    send_time = now.tv_sec;
}

static void send_task_func(void *param)
{
    while (true) {
        xSemaphoreTake(send_sem, portMAX_DELAY);
        xSemaphoreTake(send_mutex, portMAX_DELAY);
        measure_and_send(true);
        arm_send_timer();
        xSemaphoreGive(send_mutex);
    }
}

void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(nvs_open(STORAGE_NAME, NVS_READWRITE, &storage));

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    gpio_set_direction(BLE_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LORA_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(ERR_LED, GPIO_MODE_OUTPUT);

    spi_init();
    i2c_init();
    bme_init();
    lora_init(HSPI_HOST, RFM_NSS, RFM_RXTX, RFM_RESET, RFM_DIO0, RFM_DIO1);

    ble_task_done_sem = xSemaphoreCreateBinary();
    join_task_done_sem = xSemaphoreCreateBinary();
    send_sem = xSemaphoreCreateBinary();
    join_mutex = xSemaphoreCreateMutex();
    send_mutex = xSemaphoreCreateMutex();

    //init done

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        lora_set_session(dev_addr, nwk_key, art_key, seqno_up, seqno_dw);
        measure_and_send(false);
    } else {
        esp_timer_create_args_t timer_args = { .callback = &send_timer_callback, .name = "send_timer" };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &send_timer));

        xTaskCreate(send_task_func, "send_task", 4 * 1024, NULL, 10, &send_task);
        if (dev_addr) {
            lora_set_session(dev_addr, nwk_key, art_key, seqno_up, seqno_dw);
            xSemaphoreGive(join_task_done_sem);

            arm_send_timer();
        } else {
            xTaskCreate(join_task_func, "join_task", 4 * 1024, NULL, 10, &join_task);
        }

        gpio_pad_select_gpio(BLE_BUTTON);
        gpio_set_direction(BLE_BUTTON, GPIO_MODE_INPUT);
        gpio_set_pull_mode(BLE_BUTTON, GPIO_PULLUP_ONLY);
        gpio_set_intr_type(BLE_BUTTON, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(BLE_BUTTON, button_isr_handler, NULL);

        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
            xTaskCreate(ble_task_func, "ble_task", 4 * 1024, NULL, 10, NULL);
        } else {
            xSemaphoreGive(ble_task_done_sem);
        }

        xSemaphoreTake(join_task_done_sem, portMAX_DELAY);
        xSemaphoreTake(ble_task_done_sem, portMAX_DELAY);
        xSemaphoreTake(send_mutex, portMAX_DELAY);
        vTaskDelete(send_task);
        xSemaphoreGive(send_mutex);
    }

    lora_get_counters(&seqno_up, &seqno_dw);
    lora_deinit();

    esp_sleep_enable_ext0_wakeup(BLE_BUTTON, 0);
    if (dev_addr) { //only if has session
        uint16_t period;
        nvs_get_u16(storage, STORAGE_KEY_PERIOD, &period);
        struct timeval now;
        gettimeofday(&now, NULL);

        uint16_t dt = now.tv_sec - send_time;
        if (dt < period) {
            period -= dt;
        }

        ESP_LOGI(TAG, "Gonna sleep for %ds", period);
        esp_sleep_enable_timer_wakeup(period * 1000000);
    }
    esp_deep_sleep_start();
}
