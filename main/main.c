#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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
#include "battery.h"
#include "profile.h"

#define BLE_CONNECTION_TIMEOUT  60000  // 60sec
#define LORA_JOIN_TIMEOUT       900000 // 15min
#define LIRA_ERROR_TIMEOUT      30000  // 30se

static const char *TAG = "main";

static RTC_DATA_ATTR struct timeval send_time;

static esp_timer_handle_t send_timer = NULL;

static TaskHandle_t join_task;
static TaskHandle_t periodic_execute_task;

static SemaphoreHandle_t join_task_done_sem;
static SemaphoreHandle_t ble_task_done_sem;
static SemaphoreHandle_t send_sem;
static SemaphoreHandle_t join_mutex;
static SemaphoreHandle_t periodic_execute_mutex;

nvs_handle storage;

static uint64_t get_timer_timeout()
{
    uint16_t period = 60; // default 60s period
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

    size_t join_eui_len = 8 * sizeof(uint8_t);
    size_t dev_eui_len = 8 * sizeof(uint8_t);
    size_t nwk_key_len = 16 * sizeof(uint8_t);
    if (nvs_get_blob(storage, STORAGE_KEY_LORA_JOIN_EUI, NULL, &join_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_LORA_DEV_EUI, NULL, &dev_eui_len) == ESP_OK
            && nvs_get_blob(storage, STORAGE_KEY_LORA_NWK_KEY, NULL, &nwk_key_len) == ESP_OK) {
        ESP_LOGI(TAG, "Gonna to login");

        led_set_state(LED_ID_LORA, LED_STATE_DUTY_50);
        if (lora_join()) {
            led_set_state(LED_ID_LORA, LED_STATE_OFF);
            ESP_LOGI(TAG, "Login successful");
            xSemaphoreGive(send_sem);
        } else {
            led_set_state(LED_ID_LORA, LED_STATE_DUTY_90);
            ESP_LOGE(TAG, "Login not successful");
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }
    } else {
        led_set_state(LED_ID_LORA, LED_STATE_DUTY_90);
        ESP_LOGW(TAG, "Missing login credentials");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
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

    led_set_state(LED_ID_BLE, LED_STATE_DUTY_50);

    profile_measure();
    profile_send_ble();

    while ((receved = xQueueReceive(ble_event_queue, &event, BLE_CONNECTION_TIMEOUT / portTICK_PERIOD_MS)) || connection) {
        if (!receved) continue;
        switch (event) {
            case BLE_EVENT_CONNECT:
                connection = true;
                led_set_state(LED_ID_BLE, LED_STATE_ON);
                break;
            case BLE_EVENT_DISCONNECT:
                connection = false;
                led_set_state(LED_ID_BLE, LED_STATE_DUTY_50);
                break;
            case BLE_EVENT_PERIOD_UPDATE:
                if (lora_is_joined()) {
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

static void periodic_execute_func(void *param)
{
    while (true) {
        xSemaphoreTake(send_sem, portMAX_DELAY);
        xSemaphoreTake(periodic_execute_mutex, portMAX_DELAY);

        led_set_state(LED_ID_LORA, LED_STATE_DUTY_5);

        gettimeofday(&send_time, NULL);
        profile_measure();
        if (ble_has_context()) profile_send_ble();
        profile_send_lora();

        led_set_state(LED_ID_LORA, LED_STATE_OFF);

        esp_timer_start_once(send_timer, get_timer_timeout());
        xSemaphoreGive(periodic_execute_mutex);
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

    ble_task_done_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(ble_task_done_sem);
    join_task_done_sem = xSemaphoreCreateBinary();
    xSemaphoreGive(join_task_done_sem);
    send_sem = xSemaphoreCreateBinary();
    join_mutex = xSemaphoreCreateMutex();
    periodic_execute_mutex = xSemaphoreCreateMutex();

// @formatter:off
    esp_timer_create_args_t timer_args = {
            .callback = &send_timer_callback,
            .name = "send_timer"
    };
// @formatter:on
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &send_timer));

    gpio_pad_select_gpio(BUTTON_BLE);
    gpio_set_direction(BUTTON_BLE, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_BLE, GPIO_PULLUP_ONLY);
    gpio_set_intr_type(BUTTON_BLE, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(BUTTON_BLE, button_isr_handler, NULL);

    xTaskCreate(periodic_execute_func, "periodic_execute_task", 2 * 1024, NULL, 10, &periodic_execute_task);

    led_init();
    battery_measure_init();
#if (SENSOR_TYPE == SENSOR_DHT10)
    i2c_init();
#endif
    sensor_init();
    spi_init();
    lora_init();
    profile_init();

    //init done
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        xSemaphoreGive(send_sem);
    } else {
        if (lora_is_joined()) {
            xSemaphoreGive(join_task_done_sem);

            esp_timer_start_once(send_timer, get_timer_timeout());
        } else {
            xSemaphoreTake(join_task_done_sem, portMAX_DELAY);
            xTaskCreate(join_task_func, "join_task", 2 * 1024, NULL, 10, &join_task);
        }
    }

    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
        xSemaphoreTake(ble_task_done_sem, portMAX_DELAY);
        xTaskCreate(ble_task_func, "ble_task", 2 * 1024, NULL, 10, NULL);
    }

    //wait to finish all stuff...
    xSemaphoreTake(join_task_done_sem, portMAX_DELAY);
    xSemaphoreTake(ble_task_done_sem, portMAX_DELAY);
    xSemaphoreTake(periodic_execute_mutex, portMAX_DELAY);
    vTaskDelete(periodic_execute_task);
    xSemaphoreGive(periodic_execute_mutex);

    lora_deinit();
    led_deinit();
    profile_deinit();

    esp_sleep_enable_ext0_wakeup(BUTTON_BLE, 0);
    if (lora_is_joined()) { // only if has session
        esp_sleep_enable_timer_wakeup(get_timer_timeout());
    }
    ESP_LOGI(TAG, "Entering to deep sleep (run time %d)...", xTaskGetTickCount() * portTICK_PERIOD_MS);
    esp_deep_sleep_start();
}
