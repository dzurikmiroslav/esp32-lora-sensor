#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/timer.h"
#include "lmic.h"

#include "lmic_hal.h"
#include "lora.h"

static const char *TAG = "lmic_hal";

typedef enum
{
    WAKE_USER, WAKE_TIMER, WAKE_DIO0, WAKE_DIO1
} wake_t;

static SemaphoreHandle_t mutex;

static QueueHandle_t wake_queue;

static uint64_t next_timer_alarm = 0x200000000;

static const ostime_t OVERRUN_TRESHOLD = 0x10000;

static spi_device_handle_t spi_handle;

lmic_pinmap lmic_pins;

static void IRAM_ATTR timer_isr_handler(void *arg)
{
    TIMERG0.int_clr_timers.t0 = 1;

    wake_t wake = WAKE_TIMER;

    BaseType_t higherPrioTaskWoken = pdFALSE;
    xQueueSendFromISR(wake_queue, &wake, &higherPrioTaskWoken);
    if (higherPrioTaskWoken) portYIELD_FROM_ISR();
}

static void IRAM_ATTR dio_isr_handler(void *arg)
{
    wake_t wake = (wake_t) (int) arg;

    BaseType_t higherPrioTaskWoken = pdFALSE;
    xQueueSendFromISR(wake_queue, &wake, &higherPrioTaskWoken);
    if (higherPrioTaskWoken) portYIELD_FROM_ISR();
}

void hal_init_ex(const void *pContext)
{
    wake_queue = xQueueCreate(12, sizeof(wake_t));
    mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(mutex, 0);

    ESP_ERROR_CHECK(lmic_pins.nss == LORA_UNUSED_PIN);
    ESP_ERROR_CHECK(lmic_pins.dio0 == LORA_UNUSED_PIN);
    ESP_ERROR_CHECK(lmic_pins.dio1 == LORA_UNUSED_PIN);

    if (lmic_pins.rxtx != LORA_UNUSED_PIN) {
        gpio_pad_select_gpio(lmic_pins.rxtx);
        gpio_set_level((gpio_num_t) lmic_pins.rxtx, 0);
        gpio_set_direction((gpio_num_t) lmic_pins.rxtx, GPIO_MODE_OUTPUT);
    }

    if (lmic_pins.rst != LORA_UNUSED_PIN) {
        gpio_pad_select_gpio((gpio_num_t) lmic_pins.rst);
        gpio_set_level((gpio_num_t) lmic_pins.rst, 1);
        gpio_set_direction((gpio_num_t) lmic_pins.rst, GPIO_MODE_OUTPUT);
    }

    gpio_pad_select_gpio(lmic_pins.dio0);
    gpio_set_direction((gpio_num_t) lmic_pins.dio0, GPIO_MODE_INPUT);
    gpio_set_intr_type((gpio_num_t) lmic_pins.dio0, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add((gpio_num_t) lmic_pins.dio0, dio_isr_handler, (void*) (int) WAKE_DIO0);

    gpio_pad_select_gpio((gpio_num_t) lmic_pins.dio1);
    gpio_set_direction((gpio_num_t) lmic_pins.dio1, GPIO_MODE_INPUT);
    gpio_set_intr_type((gpio_num_t) lmic_pins.dio1, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add((gpio_num_t) lmic_pins.dio1, dio_isr_handler, (void*) (int) WAKE_DIO1);

    /* @formatter:off */
    spi_device_interface_config_t spi_dev_cfg = {
        .mode = 1,
        .clock_speed_hz = 10000000,
        .command_bits = 0,
        .address_bits = 8,
        .spics_io_num = lmic_pins.nss,
        .queue_size = 1,
        .cs_ena_posttrans = 2
    };
    /* @formatter:on */
    ESP_ERROR_CHECK(spi_bus_add_device(lmic_pins.spi_host, &spi_dev_cfg, &spi_handle));

    /* @formatter:off */
    timer_config_t timer_config = {
            .alarm_en = false,
            .counter_en = false,
            .intr_type = TIMER_INTR_LEVEL,
            .counter_dir = TIMER_COUNT_UP,
            .auto_reload = false,
            .divider = 1280
    };
    /* @formatter:on */
    timer_init(TIMER_GROUP_0, TIMER_0, &timer_config);
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_isr_handler, NULL, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, TIMER_0);
}

void hal_pin_rxtx(u1_t val)
{
    if (lmic_pins.rxtx != LORA_UNUSED_PIN) {
        gpio_set_level((gpio_num_t) lmic_pins.rxtx, val);
    }
}

void hal_pin_rst(u1_t val)
{
    if (lmic_pins.rst != LORA_UNUSED_PIN) {
        if (val == 0 || val == 1) {
            gpio_set_level((gpio_num_t) lmic_pins.rst, val);
            gpio_set_direction((gpio_num_t) lmic_pins.rst, GPIO_MODE_OUTPUT);
        } else {
            gpio_set_level((gpio_num_t) lmic_pins.rst, val);
            gpio_set_direction((gpio_num_t) lmic_pins.rst, GPIO_MODE_INPUT);
        }
    }
}

void hal_spi_write(u1_t cmd, const u1_t *buf, size_t len)
{
    /* @formatter:off */
    spi_transaction_t t = {
            .addr = cmd,
            .length = 8 * len,
            .tx_buffer = buf
    };
    /* @formatter:on */
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));
}

void hal_spi_read(u1_t cmd, u1_t *buf, size_t len)
{
    memset(buf, 0, len);
    /* @formatter:off */
    spi_transaction_t t = {
            .addr = cmd,
            .length = 8 * len,
            .rxlength = 8 * len,
            .tx_buffer = buf,
            .rx_buffer = buf
    };
    /* @formatter:on */
    ESP_ERROR_CHECK(spi_device_transmit(spi_handle, &t));
}

s1_t hal_getRssiCal(void)
{
    return 0;
}

ostime_t hal_setModuleActive(bit_t val)
{
    return 0;
}

bit_t hal_queryUsingTcxo(void)
{
    return false;
}

void hal_disableIRQs(void)
{
}

void hal_enableIRQs(void)
{
}

u4_t hal_ticks(void)
{
    uint64_t val;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &val);
    return (u4_t) val;
}

static void set_next_timer_alarm(u4_t time)
{
    uint64_t now;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &now);
    u4_t now32 = (u4_t) now;

    if (now != now32) {
        // decrease timer to 32 bit value
        now = now32;
        timer_pause(TIMER_GROUP_0, TIMER_0);
        timer_set_counter_value(TIMER_GROUP_0, TIMER_0, now);
        timer_start(TIMER_GROUP_0, TIMER_0);
    }

    next_timer_alarm = time;
}

static void arm_timer_alarm()
{
    timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_DIS);
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, next_timer_alarm);
    timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_EN);
}

static void disarm_timer_alarm()
{
    timer_set_alarm(TIMER_GROUP_0, TIMER_0, TIMER_ALARM_DIS);
    next_timer_alarm = 0x200000000;
}

u4_t hal_waitUntil(u4_t time)
{
    set_next_timer_alarm(time);
    hal_sleep();

    return 0;
}

u1_t hal_checkTimer(u4_t time)
{
    uint64_t now;
    timer_get_counter_value(TIMER_GROUP_0, TIMER_0, &now);
    u4_t now32 = (u4_t) now;

    if (time >= now32) {
        if (time - now32 < 5) return 1; // timer will expire very soon
    } else {
        if (now32 - time < OVERRUN_TRESHOLD) return 1; // timer has expired recently
    }

    set_next_timer_alarm(time);
    return 0;
}

void hal_sleep(void)
{
    arm_timer_alarm();

    wake_t wake;

    xSemaphoreGive(mutex);
    xQueueReceive(wake_queue, &wake, portMAX_DELAY);
    xSemaphoreTake(mutex, portMAX_DELAY);

    switch (wake) {
        case WAKE_DIO0:
            radio_irq_handler(0);
            break;
        case WAKE_DIO1:
            radio_irq_handler(1);
            break;
        default:
            break;
    }

    disarm_timer_alarm();
}

void hal_failed(const char *file, u2_t line)
{
    ESP_LOGE(TAG, "%s %d", file, line);
    ESP_ERROR_CHECK(1);
}

void lmic_hal_free()
{
    disarm_timer_alarm();

    vQueueDelete(wake_queue);
    vSemaphoreDelete(mutex);

    ESP_ERROR_CHECK(spi_bus_remove_device(spi_handle));
}

void lmic_hal_enter_critical()
{
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void lmic_hal_leave_critical()
{
    xSemaphoreGive(mutex);
}

void lmic_hal_wakeup()
{
    wake_t wake = WAKE_USER;
    xQueueSend(wake_queue, &wake, 0);
}

uint8_t hal_getTxPowerPolicy(u1_t inputPolicy, s1_t requestedPower, u4_t frequency)
{
    return LMICHAL_radio_tx_power_policy_paboost;
}
