#include "profile_soil_moisture.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "esp_adc_cal.h"

#include "peripherals.h"
#include "bme.h"
#include "ble.h"
#include "lora.h"
#include "storage_key.h"
#include "cayenne.h"

static const char *TAG = "profile_soil_mosture";

typedef struct
{
    uint16_t humidity;
    int16_t temperature;
    uint32_t pressure;
    uint8_t battery;
    uint16_t soil_moisture;
}__attribute__((packed)) native_payload_t;

static esp_adc_cal_characteristics_t adc_char;

void soil_mousture_init()
{
    gpio_pad_select_gpio(EXT_NSW);
    gpio_set_direction(EXT_NSW, GPIO_MODE_OUTPUT);
    gpio_set_level(EXT_NSW, 0);

    gpio_pad_select_gpio(EXT_0);
    gpio_set_direction(EXT_0, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(EXT_2_ADC1_CHN, ADC_ATTEN_DB_11);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_char);
}

void soil_mousture_execute(bool lora, bool ble)
{
    bme_data_t bme_data = bme_read();
    gpio_set_level(EXT_NSW, 1);
    vTaskDelay(500 / portTICK_PERIOD_MS);

    int adc_reading = adc1_get_raw(EXT_2_ADC1_CHN);
    ESP_LOGI(TAG, "Voltage %d %dmV", adc_reading, esp_adc_cal_raw_to_voltage(adc_reading, &adc_char));
    vTaskDelay(500 / portTICK_PERIOD_MS);
    adc1_get_raw(EXT_2_ADC1_CHN);
    ESP_LOGI(TAG, "Voltage %d %dmV", adc_reading, esp_adc_cal_raw_to_voltage(adc_reading, &adc_char));
    vTaskDelay(500 / portTICK_PERIOD_MS);
    adc1_get_raw(EXT_2_ADC1_CHN);
    ESP_LOGI(TAG, "Voltage %d %dmV", adc_reading, esp_adc_cal_raw_to_voltage(adc_reading, &adc_char));

    gpio_set_level(EXT_NSW, 0);

    if (ble) {
        ble_set_enviromental(bme_data.humidity, bme_data.temperature, bme_data.pressure);
        //TODO ble_set_soil_mosture
    }

    if (lora) {
        uint8_t format = 0;
        nvs_get_u8(storage, STORAGE_KEY_PAYL_FMT, &format);

        if (format == 1) {
            //cayenne lpp payload
            uint8_t humidity_val = bme_data.humidity * 2;
            int16_t temperature_val = bme_data.temperature * 10;
            uint16_t pressure_val = bme_data.pressure / 100 * 10;

            uint8_t len = 0;
            uint8_t payload[14];
            payload[len++] = 1;
            payload[len++] = CAYENNE_LPP_RELATIVE_HUMIDITY;
            payload[len++] = humidity_val;
            payload[len++] = 2;
            payload[len++] = CAYENNE_LPP_TEMPERATURE;
            payload[len++] = temperature_val >> 8;
            payload[len++] = temperature_val;
            payload[len++] = 3;
            payload[len++] = CAYENNE_LPP_BAROMETRIC_PRESSURE;
            payload[len++] = pressure_val >> 8;
            payload[len++] = pressure_val;
            payload[len++] = 4;
            payload[len++] = CAYENNE_LPP_DIGITAL_OUTPUT;
            payload[len++] = 100;

            lora_send(payload, len);
        } else {
            //native payload
            native_payload_t payload;
            payload.humidity = bme_data.humidity * 100;
            payload.temperature = bme_data.temperature * 100;
            payload.pressure = bme_data.pressure * 100;
            payload.battery = 100;

            lora_send((uint8_t*) &payload, sizeof(native_payload_t));
        }
    }
}

void soil_mousture_deinit()
{
    //gpio_pad_select_gpio(EXT_NSW);
    gpio_set_direction(EXT_NSW, GPIO_MODE_DISABLE);
}
