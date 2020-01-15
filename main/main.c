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
#include "sensor.h"
#include "peripherals.h"
#include "profile.h"
#include "battery.h"

#define BLE_CONNECTION_TIMEOUT  60000  /* 60sec */
#define LORA_JOIN_TIMEOUT       900000 /* 15min */
#define LIRA_ERROR_TIMEOUT      30000  /* 30sec */

static const char *TAG = "main";

static RTC_DATA_ATTR uint32_t dev_addr = 0;
static RTC_DATA_ATTR uint8_t nwk_key[16];
static RTC_DATA_ATTR uint8_t art_key[16];
static RTC_DATA_ATTR uint32_t seqno_up;
static RTC_DATA_ATTR uint32_t seqno_dw;
static RTC_DATA_ATTR struct timeval send_time;

static esp_timer_handle_t send_timer = NULL;

static TaskHandle_t join_task;
static TaskHandle_t send_task;

static SemaphoreHandle_t join_task_done_sem;
static SemaphoreHandle_t ble_task_done_sem;
static SemaphoreHandle_t send_sem;
static SemaphoreHandle_t join_mutex;
static SemaphoreHandle_t send_mutex;

nvs_handle storage;

static profile_callbacks_t profile_cb;

static uint64_t get_timer_timeout()
{
    uint16_t period = 60; //default 60s period
    nvs_get_u16(storage, STORAGE_KEY_PERIOD, &period);

    uint64_t timeout = period * 1000000;

    struct timeval now;
    gettimeofday(&now, NULL);

    uint64_t dt = (now.tv_sec * 1000000 + now.tv_usec) - (send_time.tv_sec * 1000000 + send_time.tv_usec);
    if (dt < timeout) {
        timeout -= dt;
    }

    ESP_LOGI(TAG, "Timer timeout %llu s", timeout / 1000000);

    return timeout;
}

static void join_task_func(void *param)
{
    esp_timer_stop(send_timer);
    memset(&send_time, 0, sizeof(struct timeval));
    dev_addr = 0;

    led_set_state(LED_ID_ERR, LED_STATE_OFF);

    size_t app_eui_len = 8;
    size_t dev_eui_len = 8;
    size_t dev_key_len = 16;
    if (nvs_get_blob(storage, STORAGE_KEY_APP_EUI, NULL, &app_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_DEV_EUI, NULL, &dev_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_DEV_KEY, NULL, &dev_key_len) == ESP_OK) {
        ESP_LOGI(TAG, "Gonna to login");
        xSemaphoreTake(join_mutex, portMAX_DELAY);
        lora_start_joining();
        led_set_state(LED_ID_LORA, LED_STATE_FLASH);
        xSemaphoreGive(join_mutex);

        bool join_status;
        if (xQueueReceive(lora_join_queue, &join_status, LORA_JOIN_TIMEOUT / portTICK_PERIOD_MS) && join_status) {
            led_set_state(LED_ID_LORA, LED_STATE_OFF);
            ESP_LOGI(TAG, "Login successful");
            lora_get_session(&dev_addr, nwk_key, art_key);
            xSemaphoreGive(send_sem);
        } else {
            led_set_state(LED_ID_LORA, LED_STATE_OFF);
            led_set_state(LED_ID_ERR, LED_STATE_ON);
            ESP_LOGE(TAG, "Login not successful");
            vTaskDelay(30000 / portTICK_PERIOD_MS);
        }
    } else {
       led_set_state(LED_ID_ERR, LED_STATE_FLASH);
       ESP_LOGW(TAG, "Missing login credentials");
       vTaskDelay(30000 / portTICK_PERIOD_MS);
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

    ble_init();

    led_set_state(LED_ID_BLE, LED_STATE_FLASH);

    profile_cb.execute(false, true);

    while ((receved = xQueueReceive(ble_event_queue, &event, BLE_CONNECTION_TIMEOUT / portTICK_PERIOD_MS)) || connection) {
        if (!receved) continue;
        switch (event) {
            case BLE_EVENT_CONNECT:
                connection = true;
                led_set_state(LED_ID_BLE, LED_STATE_ON);
                break;
            case BLE_EVENT_DISCONNECT:
                connection = false;
                led_set_state(LED_ID_BLE, LED_STATE_FLASH);
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
            case BLE_EVENT_PROFILE_UPDATE:
                profile_cb.deinit();
                uint8_t profile = 0;
                nvs_get_u8(storage, STORAGE_KEY_PROFILE, &profile);
                ESP_LOGI(TAG, "Changing to profile %u", profile);
                profile_cb = profile_get_callbacks(profile);
                profile_cb.init();
                break;
            default:
                break;
        }
    }

    ble_deinit();
    xSemaphoreGive(ble_task_done_sem);
    led_set_state(LED_ID_BLE, LED_STATE_OFF);
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
    led_set_state(LED_ID_LORA, LED_STATE_FLASH);

    gettimeofday(&send_time, NULL);
    profile_cb.execute(dev_addr, has_ble);

    led_set_state(LED_ID_LORA, LED_STATE_OFF);
}

static void send_task_func(void *param)
{
    while (true) {
        xSemaphoreTake(send_sem, portMAX_DELAY);
        xSemaphoreTake(send_mutex, portMAX_DELAY);
        measure_and_send(true);
        esp_timer_start_once(send_timer, get_timer_timeout());
        xSemaphoreGive(send_mutex);
    }
}

void app_main()
{
    ESP_LOGI(TAG, "Starting...");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(nvs_open(STORAGE_NAME, NVS_READWRITE, &storage));

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    uint8_t profile = 0;
    nvs_get_u8(storage, STORAGE_KEY_PROFILE, &profile);
    profile_cb = profile_get_callbacks(profile);
    ESP_LOGI(TAG, "Profile %u", profile);

    led_init();
    battery_measure_init();
#if (SENSOR_TYPE == SENSOR_DHT10)
    i2c_init();
#endif
    sensor_init();

    spi_init(SPI_MISO, SPI_MOSI, SPI_SCLK);
    lora_init(HSPI_HOST, SPI_RFM_NSS, LORA_UNUSED_PIN, SPI_RFM_RESET, SPI_RFM_DIO0, SPI_RFM_DIO1);

    profile_cb.init();

    ble_task_done_sem = xSemaphoreCreateBinary();
    join_task_done_sem = xSemaphoreCreateBinary();
    send_sem = xSemaphoreCreateBinary();
    join_mutex = xSemaphoreCreateMutex();
    send_mutex = xSemaphoreCreateMutex();

    //init done

    uint64_t init_time = 0;

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        lora_set_session(dev_addr, nwk_key, art_key, seqno_up, seqno_dw);
        init_time = esp_timer_get_time();
        measure_and_send(false);
    } else {
        // @formatter:off
        esp_timer_create_args_t timer_args = {
                .callback = &send_timer_callback,
                .name = "send_timer"
        };
        // @formatter:on
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &send_timer));

        xTaskCreate(send_task_func, "send_task", 4 * 1024, NULL, 10, &send_task);
        if (dev_addr) {
            lora_set_session(dev_addr, nwk_key, art_key, seqno_up, seqno_dw);
            xSemaphoreGive(join_task_done_sem);

            esp_timer_start_once(send_timer, get_timer_timeout());
        } else {
            xTaskCreate(join_task_func, "join_task", 4 * 1024, NULL, 10, &join_task);
        }

        gpio_pad_select_gpio(BUTTON_BLE);
        gpio_set_direction(BUTTON_BLE, GPIO_MODE_INPUT);
        gpio_set_pull_mode(BUTTON_BLE, GPIO_PULLUP_ONLY);
        gpio_set_intr_type(BUTTON_BLE, GPIO_INTR_NEGEDGE);
        gpio_isr_handler_add(BUTTON_BLE, button_isr_handler, NULL);

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

    profile_cb.deinit();

    esp_sleep_enable_ext0_wakeup(BUTTON_BLE, 0);
    if (dev_addr) { //only if has session
        esp_sleep_enable_timer_wakeup(get_timer_timeout() - init_time);
    }
    ESP_LOGI(TAG, "Entering to deep sleep (run time %d)...", xTaskGetTickCount() * portTICK_PERIOD_MS);
    esp_deep_sleep_start();
}
