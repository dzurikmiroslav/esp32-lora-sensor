#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/ledc.h"

#include "peripherals.h"

#define I2C_MASTER_NUM      1
#define I2C_MASTER_FREQ_HZ  100000
#define ACK_CHECK_EN        0x1 /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS       0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL             0x0 /*!< I2C ack value */
#define NACK_VAL            0x1 /*!< I2C nack value */

static ledc_channel_t led_channel[] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1 };

void spi_init()
{
    spi_bus_config_t spi_cfg = { 0 };
    spi_cfg.miso_io_num = SPI_MISO;
    spi_cfg.mosi_io_num = SPI_MOSI;
    spi_cfg.sclk_io_num = SPI_SCLK;
    spi_cfg.quadwp_io_num = -1;
    spi_cfg.quadhd_io_num = -1;
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &spi_cfg, 1));
}

void i2c_init()
{
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_SCL;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    ESP_ERROR_CHECK(i2c_param_config(i2c_master_port, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0));
}

int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_id << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    if (len > 0) {
        i2c_master_write(cmd, reg_data, len, ACK_CHECK_EN);
    }
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK ? 0 : -1;
}

int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    if (len == 0) {
        return 0;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_id << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg_addr, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    if (ret != ESP_OK) {
        return -1;
    }
    i2c_cmd_link_delete(cmd);

    vTaskDelay(30 / portTICK_RATE_MS);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (dev_id << 1) | I2C_MASTER_READ, ACK_CHECK_EN);
    if (len > 1) {
        i2c_master_read(cmd, reg_data, len - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, reg_data + len - 1, NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret == ESP_OK ? 0 : -1;
}

void led_init()
{
    /* @formatter:off */
    ledc_timer_config_t ledc_timer = {
            .duty_resolution = LEDC_TIMER_10_BIT,    //1023
            .freq_hz    = 1,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .timer_num  = LEDC_TIMER_0
    };
                        /* @formatter:on */
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    /* @formatter:off */
    ledc_channel_config_t ledc_channel = {
            .gpio_num   = LED_BLE,
            .channel    = LEDC_CHANNEL_0,
            .speed_mode = LEDC_HIGH_SPEED_MODE,
            .intr_type  = LEDC_INTR_DISABLE,
            .timer_sel  = LEDC_TIMER_0,
            .duty       = 0
    };
                        /* @formatter:on */
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.gpio_num = LED_LORA;
    ledc_channel.channel = LEDC_CHANNEL_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    //ledc_fade_func_install(0);
}

void led_deinit()
{
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_1, 0);
}

void led_set_state(led_id_t led_id, led_state_t state)
{
    switch (state) {
        case LED_STATE_ON:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id], 1023);
            break;
        case LED_STATE_OFF:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id], 0);
            break;
        case LED_STATE_DUTY_5:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id], 1023 / 100 * 5);
            break;
        case LED_STATE_DUTY_50:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id], 1023 / 100 * 50);
            break;
        case LED_STATE_DUTY_90:
            ledc_set_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id], 1023 / 100 * 75);
            break;
    }
    ledc_update_duty(LEDC_HIGH_SPEED_MODE, led_channel[led_id]);
}
