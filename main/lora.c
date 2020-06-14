#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "nvs.h"
#include "esp32/rom/rtc.h"
#include "soc/rtc.h"
#include "ldl_mac.h"
#include "ldl_radio.h"
#include "ldl_sm.h"
#include "ldl_system.h"

#include "storage_key.h"
#include "peripherals.h"

#define TPS 1000000UL  /* ticks per second (microsecond) */
#define DIVIDER 128

static const char *TAG = "lora";

static spi_device_handle_t spi_handle;

static TaskHandle_t process_task;

static QueueHandle_t wake_queue;
static SemaphoreHandle_t send_semhr;
static QueueHandle_t join_queue;

static RTC_DATA_ATTR struct ldl_sm sm;
static RTC_DATA_ATTR struct ldl_mac_session mac_session = {0};
static struct ldl_radio radio;
static struct ldl_mac mac;

typedef enum
{
    WAKE_NONE, WAKE_JOIN, WAKE_SEND
} wake_t;

static uint8_t send_buffer[LDL_MAX_PACKET];
static uint8_t send_len = 0;
static uint8_t send_confirmed;

uint32_t LDL_System_ticks(void *app)
{
        uint64_t val;
        timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &val);
        return (uint32_t) val;
//    return esp_timer_get_time();
}

uint8_t LDL_System_getBatteryLevel(void *app)
{
    return 100;
}

uint32_t LDL_System_tps(void)
{
   return (((READ_PERI_REG(RTC_APB_FREQ_REG)) & UINT16_MAX) << 12) / DIVIDER;
//    return TPS;
}

uint32_t LDL_System_eps(void)
{
    return 5000;
}

void LDL_Chip_reset(void *self, bool state)
{
    gpio_set_direction(RFM_RESET, state ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

void LDL_Chip_write(void *self, uint8_t addr, const uint8_t *data, uint8_t len)
{
// @formatter:off
	spi_transaction_t t = {
			.addr = addr | 0x80U,
			.length = 8 * len,
			.tx_buffer = data
	};
// @formatter:on
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));
}

void LDL_Chip_read(void *self, uint8_t addr, uint8_t *data, uint8_t len)
{
    memset(data, 0, len);
// @formatter:off
    spi_transaction_t t = {
            .addr = addr & 0x7fU,
            .length = 8 * len,
            .rxlength = 8 * len,
            .tx_buffer = data,
            .rx_buffer = data
    };
// @formatter:on
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));
}

void ldl_handler(void *app, enum ldl_mac_response_type type, const union ldl_mac_response_arg *arg)
{
    bool join_status;
    switch (type) {
        case LDL_MAC_CHIP_ERROR:
            break;
        case LDL_MAC_STARTUP:
            srand(arg->startup.entropy);
            break;
        case LDL_MAC_JOIN_COMPLETE:
#ifdef CONFIG_LORA_LORAWAN_VERSION_1_1
            nvs_set_u16(storage, STORAGE_KEY_LORA_DEV_NONCE, arg->join_complete.nextDevNonce);
            nvs_set_u32(storage, STORAGE_KEY_LORA_JOIN_NONCE, arg->join_complete.joinNonce);
#endif /* CONFIG_LORA_LORAWAN_VERSION_1_1 */
            join_status = true;
            xQueueSend(join_queue, &join_status, portMAX_DELAY);
            break;
        case LDL_MAC_JOIN_TIMEOUT:
            join_status = false;
            xQueueSend(join_queue, &join_status, portMAX_DELAY);
            break;
        case LDL_MAC_RX1_SLOT:
            ESP_LOGI(TAG, "RX1 slot");
            break;
        case LDL_MAC_RX2_SLOT:
            ESP_LOGI(TAG, "RX2 slot");
            break;
        case LDL_MAC_DOWNSTREAM:
            ESP_LOGI(TAG, "Downstrean");
            break;
        case LDL_MAC_TX_COMPLETE:
            ESP_LOGI(TAG, "TX complete");
            break;
        case LDL_MAC_TX_BEGIN:
            ESP_LOGI(TAG, "TX begin");
            break;
        case LDL_MAC_SESSION_UPDATED:
            if (arg->session_updated.session->joined) {
                memcpy(&mac_session, arg->session_updated.session, sizeof(struct ldl_mac_session));
            } else {
                memset(&mac_session, 0, sizeof(struct ldl_mac_session));
            }
            xSemaphoreGive(send_semhr);
            break;
        default:
            break;
    }
}

static void IRAM_ATTR dio_isr_handler(void *arg)
{
    uint8_t dio = (uint32_t) arg;
    LDL_Radio_interrupt(&radio, dio);
}

static void process_func(void *param)
{
    wake_t wake = WAKE_NONE;
    while (1) {
        if (LDL_MAC_ready(&mac)) {
            switch (wake) {
                case WAKE_JOIN:
                    if (!LDL_MAC_joined(&mac)) {
                        ESP_LOGI(TAG, "LDL_MAC_otaa");
                        LDL_MAC_otaa(&mac);
                    } else {
                        wake = WAKE_NONE;
                    }
                    break;
                case WAKE_SEND:
                    ESP_LOG_BUFFER_HEX_LEVEL(TAG, send_buffer, send_len, ESP_LOG_INFO);
                    if (send_confirmed) {
                        LDL_MAC_confirmedData(&mac, 1U, send_buffer, send_len,
                        NULL);
                    } else {
                        LDL_MAC_unconfirmedData(&mac, 1U, send_buffer, send_len,
                        NULL);
                    }
                    wake = WAKE_NONE;
                    break;
                default:
                    break;
            }
        }

        LDL_MAC_process(&mac);

        uint32_t ticks_until_next_event = LDL_MAC_ticksUntilNextEvent(&mac);
        //ESP_LOGI(TAG, "ticks_until_next_event %d", ticks_until_next_event);
        xQueueReceive(wake_queue, &wake, (ticks_until_next_event / ((LDL_System_tps() / 1000)) / portTICK_PERIOD_MS));
    }
}

void mac_init()
{
    size_t size;

    uint8_t app_key[16];
    size = sizeof(app_key);
    nvs_get_blob(storage, STORAGE_KEY_LORA_APP_KEY, app_key, &size);

    uint8_t nwk_key[16];
    size = sizeof(nwk_key);
    nvs_get_blob(storage, STORAGE_KEY_LORA_NWK_KEY, nwk_key, &size);

    LDL_SM_init(&sm, app_key, nwk_key);

    struct ldl_mac_init_arg arg = { 0 };
    arg.radio = &radio;
    arg.handler = ldl_handler;
    arg.sm = &sm;
    if (mac_session.joined) {
        arg.session = &mac_session;
    }
#ifdef CONFIG_LORA_LORAWAN_VERSION_1_1
    nvs_get_u16(storage, STORAGE_KEY_LORA_DEV_NONCE, &arg.devNonce);
    nvs_get_u32(storage, STORAGE_KEY_LORA_JOIN_NONCE, &arg.joinNonce);
#endif /* CONFIG_LORA_LORAWAN_VERSION_1_1 */
    arg.gain = 0;

    uint8_t join_eui[16];
    size = sizeof(join_eui);
    nvs_get_blob(storage, STORAGE_KEY_LORA_JOIN_EUI, join_eui, &size);
    arg.joinEUI = join_eui;

    uint8_t dev_eui[16];
    size = sizeof(join_eui);
    nvs_get_blob(storage, STORAGE_KEY_LORA_DEV_EUI, dev_eui, &size);
    arg.devEUI = dev_eui;

    ESP_LOGI(TAG, "Init with devNonce %d joinNonce %d", arg.devNonce, arg.joinNonce);

    LDL_MAC_init(&mac, LDL_EU_863_870, &arg);

    LDL_MAC_setMaxDCycle(&mac, 7);
}

void lora_init()
{
    wake_queue = xQueueCreate(12, sizeof(wake_t));
    send_semhr = xSemaphoreCreateBinary();
    join_queue = xQueueCreate(1, sizeof(bool));

    gpio_pad_select_gpio(RFM_RESET);
    gpio_set_direction(RFM_RESET, GPIO_MODE_INPUT);
    gpio_set_level(RFM_RESET, 0);

    gpio_pad_select_gpio(RFM_DIO0);
    gpio_set_direction(RFM_DIO0, GPIO_MODE_INPUT);
    gpio_set_intr_type(RFM_DIO0, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(RFM_DIO0, dio_isr_handler, (void*) 0);

    gpio_pad_select_gpio(RFM_DIO1);
    gpio_set_direction(RFM_DIO1, GPIO_MODE_INPUT);
    gpio_set_intr_type(RFM_DIO1, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(RFM_DIO1, dio_isr_handler, (void*) 1);

// @formatter:off
    spi_device_interface_config_t spi_dev_cfg = {
            .mode = 1,
            .clock_speed_hz = 4000000,
            .command_bits = 0,
            .address_bits = 8,
            .spics_io_num = RFM_NSS,
            .queue_size = 1,
            .cs_ena_posttrans = 2
    };
// @formatter:on
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &spi_dev_cfg, &spi_handle));

// @formatter:off
    timer_config_t timer_config = {
            .alarm_en = false,
            .counter_en = false,
            .intr_type = TIMER_INTR_LEVEL,
            .counter_dir = TIMER_COUNT_UP,
            .auto_reload = false,
            .divider = DIVIDER
    };
// @formatter:on */
    timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x0);
    timer_start(TIMER_GROUP_0, TIMER_0);

    LDL_Radio_init(&radio, LDL_RADIO_SX1276, NULL);
    LDL_Radio_setPA(&radio, LDL_RADIO_PA_BOOST);

    mac_init();

    xTaskCreate(process_func, "lora_process_task", 4 * 1024, NULL, 2, &process_task);
}

bool lora_is_joined()
{
    return LDL_MAC_joined(&mac);
}

void lora_deinit()
{
    vTaskSuspend(process_task);

    vQueueDelete(wake_queue);
    vSemaphoreDelete(send_semhr);
    vQueueDelete(join_queue);
}

bool lora_join()
{
    wake_t wake = WAKE_JOIN;
    xQueueSend(wake_queue, &wake, portMAX_DELAY);

    bool joined;
    xQueueReceive(join_queue, &joined, portMAX_DELAY);
    return joined;
}

void lora_send(const void *payload, uint8_t len)
{
    xSemaphoreTake(send_semhr, 0); //try take value from previous join event

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, payload, len, ESP_LOG_INFO);

    memcpy(&send_buffer[0], payload, len);
    send_len = len;
    nvs_get_u8(storage, STORAGE_KEY_CONFM, &send_confirmed);

    wake_t wake = WAKE_SEND;
    xQueueSend(wake_queue, &wake, portMAX_DELAY);

    xSemaphoreTake(send_semhr, portMAX_DELAY);
}
