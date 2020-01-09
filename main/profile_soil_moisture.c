#include "profile_soil_moisture.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "peripherals.h"
#include "battery.h"
#include "sensor.h"
#include "ble.h"
#include "lora.h"
#include "storage_key.h"
#include "cayenne.h"

#define GPIO_POWER      GPIO_NUM_32
#define INPUT_CHANNEL   ADC1_CHANNEL_7
#define ADC_ATTEN       ADC_ATTEN_DB_11
#define ADC_WIDTH       ADC_WIDTH_BIT_12

static const char *TAG = "profile_soil_mosture";

typedef struct
{
    uint16_t humidity;
    int16_t temperature;
    uint8_t battery;
    uint16_t soil_moisture;
}__attribute__((packed)) native_payload_t;

static esp_adc_cal_characteristics_t adc_char;

void soil_mousture_init()
{
    gpio_pad_select_gpio(GPIO_POWER);
    gpio_set_direction(GPIO_POWER, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_POWER, 0);

    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(INPUT_CHANNEL, ADC_ATTEN);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_char);
}

void soil_mousture_execute(bool lora, bool ble)
{
    uint8_t battery = battery_measure();
    float humidity, temperature;
    sensor_read(&humidity, &temperature);

    gpio_set_level(GPIO_POWER, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    int adc_reading;
    for (int i = 0; i < 30; i++) {
        adc_reading = adc1_get_raw(INPUT_CHANNEL);
        ESP_LOGI(TAG, "Voltage %d %dmV", adc_reading, esp_adc_cal_raw_to_voltage(adc_reading, &adc_char));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    gpio_set_level(GPIO_POWER, 0);

    float voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char) / 1000.0;

    if (ble) {
        ble_set_battery(battery);
        ble_set_enviromental(humidity, temperature);
        //TODO ble_set_soil_mosture
    }

    if (lora) {
        uint8_t format = 0;
        nvs_get_u8(storage, STORAGE_KEY_PAYL_FMT, &format);

        if (format == 1) {
            //cayenne lpp payload
            uint8_t humidity_val = humidity * 2;
            int16_t temperature_val = temperature * 10;
            int16_t soil_moisture = voltage * 100;
            int16_t battery_val = battery * 100;

            uint8_t len = 0;
            uint8_t payload[19];
            payload[len++] = 1;
            payload[len++] = CAYENNE_LPP_RELATIVE_HUMIDITY;
            payload[len++] = humidity_val;
            payload[len++] = 2;
            payload[len++] = CAYENNE_LPP_TEMPERATURE;
            payload[len++] = temperature_val >> 8;
            payload[len++] = temperature_val;
            payload[len++] = 3;
            payload[len++] = CAYENNE_LPP_ANALOG_INPUT;
            payload[len++] = battery >> 8;
            payload[len++] = battery;
            payload[len++] = 4;
            payload[len++] = CAYENNE_LPP_ANALOG_INPUT;
            payload[len++] = soil_moisture >> 8;
            payload[len++] = soil_moisture;

            lora_send(payload, len);
        } else {
            //native payload
            native_payload_t payload;
            payload.humidity = humidity * 100;
            payload.temperature = temperature * 100;
            payload.battery = battery;
            payload.soil_moisture = voltage * 100;

            lora_send((uint8_t*) &payload, sizeof(native_payload_t));
        }
    }
}

void soil_mousture_deinit()
{
    gpio_set_direction(GPIO_POWER, GPIO_MODE_DISABLE);
}
