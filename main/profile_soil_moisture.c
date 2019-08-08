#include "profile_soil_moisture.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "peripherals.h"
#include "battery.h"
#include "bme.h"
#include "ble.h"
#include "lora.h"
#include "storage_key.h"
#include "cayenne.h"

#define ADC_ATTEN   ADC_ATTEN_DB_11
#define ADC_WIDTH   ADC_WIDTH_BIT_12

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
    gpio_pad_select_gpio(EXT_SW);
    gpio_set_direction(EXT_SW, GPIO_MODE_OUTPUT);
    gpio_set_level(EXT_SW, 0);

    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(EXT_0_ADC1_CHN, ADC_ATTEN);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_char);
}

void soil_mousture_execute(bool lora, bool ble)
{
    uint8_t battery = battery_measure();
    bme_data_t bme_data = bme_read();

    gpio_set_level(EXT_SW, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    int adc_reading;
    for (int i = 0; i < 30; i++) {
        adc_reading = adc1_get_raw(EXT_0_ADC1_CHN);
        ESP_LOGI(TAG, "Voltage %d %dmV", adc_reading, esp_adc_cal_raw_to_voltage(adc_reading, &adc_char));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    gpio_set_level(EXT_SW, 0);

    float voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc_char) / 1000.0;

    if (ble) {
        ble_set_battery(battery);
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
            int16_t soil_moisture = voltage * 100;

            uint8_t len = 0;
            uint8_t payload[18];
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
            payload[len++] = battery;
            payload[len++] = 5;
            payload[len++] = CAYENNE_LPP_ANALOG_INPUT;
            payload[len++] = soil_moisture >> 8;
            payload[len++] = soil_moisture;

            lora_send(payload, len);
        } else {
            //native payload
            native_payload_t payload;
            payload.humidity = bme_data.humidity * 100;
            payload.temperature = bme_data.temperature * 100;
            payload.pressure = bme_data.pressure * 100;
            payload.battery = battery;
            payload.soil_moisture = voltage * 100;

            lora_send((uint8_t*) &payload, sizeof(native_payload_t));
        }
    }
}

void soil_mousture_deinit()
{
    //gpio_pad_select_gpio(EXT_NSW);
    gpio_set_direction(EXT_SW, GPIO_MODE_DISABLE);
}
