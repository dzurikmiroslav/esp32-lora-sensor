#include "bme.h"

#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "bme280.h"

#include "peripherals.h"

static const char *TAG = "bme";

#define I2C_MASTER_NUM 1
#define WRITE_BIT I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ   /*!< I2C master read */
#define ACK_CHECK_EN 0x1           /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0          /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                /*!< I2C ack value */
#define NACK_VAL 0x1               /*!< I2C nack value */

static struct bme280_dev bme280;

static void bme_delay_ms(uint32_t period)
{
    ets_delay_us(period * 1000);
}

void bme_init()
{
    bme280.write = i2c_write;
    bme280.read = i2c_read;
    bme280.delay_ms = bme_delay_ms;
    bme280.intf = BME280_I2C_INTF;
    bme280.dev_id = BME280_I2C_ADDR_PRIM;
    ESP_ERROR_CHECK(bme280_init(&bme280));

    bme280.settings.osr_h = BME280_OVERSAMPLING_1X;
    bme280.settings.osr_p = BME280_OVERSAMPLING_16X;
    bme280.settings.osr_t = BME280_OVERSAMPLING_2X;
    bme280.settings.filter = BME280_FILTER_COEFF_16;
    uint8_t settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;
    ESP_ERROR_CHECK(bme280_set_sensor_settings(settings_sel, &bme280));
}

bme_data_t bme_read()
{
    bme_data_t ret;
    struct bme280_data data;

    ESP_ERROR_CHECK(bme280_set_sensor_mode(BME280_FORCED_MODE, &bme280));
    vTaskDelay(70 / portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(bme280_get_sensor_data(BME280_ALL, &data, &bme280));

    ret.humidity = data.humidity / 1024.0f;
    ret.temperature = data.temperature / 100.0f;
    ret.pressure = data.pressure / 100.0f;

    ESP_LOGD(TAG, "Temperature %.2f °C", ret.temperature);
    ESP_LOGD(TAG, "Humidity %.2f %%", ret.humidity);
    ESP_LOGD(TAG, "Pressure %.2f Pa", ret.pressure);

    return ret;
}
