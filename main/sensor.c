#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "sdkconfig.h"

#include "sensor.h"
#include "peripherals.h"

#define ONE_WIRE                I2C_SDA

#define DHT10_I2C_ADDR          0x38
#define DHT10_RESET_REG_ADDR    0xba
#define DHT10_INIT_REG_ADDR     0xe1
#define DHT10_MEASURE_REG_ADDR  0xac

static const char *TAG = "sensor";

void sensor_init()
{
#ifdef CONFIG_SENSOR_TYPE_DHT10
    ESP_ERROR_CHECK(i2c_write(DHT10_I2C_ADDR, DHT10_RESET_REG_ADDR, NULL, 0));

    uint8_t init_param[] = { 0x08, 0x00 };
    ESP_ERROR_CHECK(i2c_write(DHT10_I2C_ADDR, DHT10_INIT_REG_ADDR, init_param, sizeof(init_param)));

    uint8_t status = 0;
    ESP_ERROR_CHECK(i2c_read(DHT10_I2C_ADDR, 1, &status, sizeof(uint8_t)));
    ESP_ERROR_CHECK((status & 0x8) == 0x8 ? ESP_OK : ESP_FAIL);
#endif /* CONFIG_SENSOR_TYPE_DHT10 */
#ifdef CONFIG_SENSOR_TYPE_DHT22
    gpio_pad_select_gpio(ONE_WIRE);
#endif /* CONFIG_SENSOR_TYPE_DHT22 */
}

#ifdef CONFIG_SENSOR_TYPE_DHT22
static esp_err_t dht_await_pin_state(gpio_num_t pin, uint32_t timeout, int expected_pin_state, uint32_t *duration)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout; i += 2)
    {
        ets_delay_us(2);
        if (gpio_get_level(pin) == expected_pin_state) {
            if (duration) *duration = i;
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

static inline int16_t dht_convert_data(uint8_t msb, uint8_t lsb)
{
    int16_t data;
    data = msb & 0x7F;
    data <<= 8;
    data |= lsb;
    if (msb & BIT(7)) data = -data;
    return data;
}
#endif /* CONFIG_SENSOR_TYPE_DHT22 */

void sensor_read(float *humidity, float *temperature)
{
#ifdef CONFIG_SENSOR_TYPE_DHT10
    uint8_t measure_param[] = { 0x33, 0x00 };
    ESP_ERROR_CHECK(i2c_write(DHT10_I2C_ADDR, DHT10_MEASURE_REG_ADDR, measure_param, sizeof(measure_param)));

    vTaskDelay(60 / portTICK_RATE_MS);

    uint8_t bytes[6] = { 0 };
    ESP_ERROR_CHECK(i2c_read(DHT10_I2C_ADDR, DHT10_MEASURE_REG_ADDR, bytes, sizeof(bytes)));

    ESP_LOG_BUFFER_HEX(TAG, bytes, 6);

    *humidity = ((((bytes[1] << 8) | bytes[2]) << 4) | bytes[3] >> 4) / 1048576.0f * 100.0f;
    *temperature = (((((bytes[3] & 0b00001111) << 8) | bytes[4]) << 8) | bytes[5]) / 1048576.0f * 200.0f - 50.0f;
#endif /* CONFIG_SENSOR_TYPE_DHT10 */
#ifdef CONFIG_SENSOR_TYPE_DHT22
    uint32_t low_duration;
    uint32_t high_duration;

    // Phase 'A' pulling signal low to initiate read sequence
    gpio_set_direction(ONE_WIRE, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(ONE_WIRE, 0);
    ets_delay_us(20000);
    gpio_set_level(ONE_WIRE, 1);

    // Step through Phase 'B', 40us
    ESP_ERROR_CHECK(dht_await_pin_state(ONE_WIRE, 40, 0, NULL));
    // Step through Phase 'C', 88us
    ESP_ERROR_CHECK(dht_await_pin_state(ONE_WIRE, 88, 1, NULL));
    // Step through Phase 'D', 88us
    ESP_ERROR_CHECK(dht_await_pin_state(ONE_WIRE, 88, 0, NULL));

    bool bits[40];

    // Read in each of the 40 bits of data...
    for (int i = 0; i < 40; i++) {
        ESP_ERROR_CHECK(dht_await_pin_state(ONE_WIRE, 65, 1, &low_duration));
        ESP_ERROR_CHECK(dht_await_pin_state(ONE_WIRE, 75, 0, &high_duration));
        bits[i] = high_duration > low_duration;
    }

    uint8_t data[40 / 8] = { 0 };
    for (uint8_t i = 0; i < 40; i++) {
        // Read each bit into 'result' byte array...
        data[i / 8] <<= 1;
        data[i / 8] |= bits[i];
    }

    *humidity = dht_convert_data(data[0], data[1]) / 10.0f;
    *temperature = dht_convert_data(data[2], data[3]) / 10.0f;
#endif /* CONFIG_SENSOR_TYPE_DHT22 */

    ESP_LOGI(TAG, "Temperature %.2f °C", *temperature);
    ESP_LOGI(TAG, "Humidity %.2f %%", *humidity);
}
