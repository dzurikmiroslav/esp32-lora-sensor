#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"

#include "profile_environmental.h"
#include "peripherals.h"
#include "battery.h"
#include "sensor.h"
#include "ble.h"
#include "lora.h"
#include "storage_key.h"
#include "cayenne.h"


typedef struct
{
    uint16_t humidity;
    int16_t temperature;
    uint8_t battery;
} __attribute__((packed)) native_payload_t;

static const char *TAG = "profile_environmental";

void environmental_init()
{
}

void environmental_execute(bool lora, bool ble)
{
    uint8_t battery;
    float battery_voltage;
    battery_measure(&battery, &battery_voltage);

    float humidity, temperature;
    sensor_read(&humidity, &temperature);

    if (ble) {
        ble_set_battery(battery);
        ble_set_enviromental(humidity, temperature);
    }

    if (lora) {
        uint8_t format = 0;
        nvs_get_u8(storage, STORAGE_KEY_PAYL_FMT, &format);

        uint8_t *payload;
        size_t length = 0;

        if (format == 1) {
            //cayenne lpp payload
            uint8_t humidity_val = humidity * 2;
            int16_t temperature_val = temperature * 10;
            int16_t battery_val = battery_voltage * 100;

            uint8_t len = 0;
            uint8_t lpp[15];
            lpp[len++] = 1;
            lpp[len++] = CAYENNE_LPP_RELATIVE_HUMIDITY;
            lpp[len++] = humidity_val;
            lpp[len++] = 2;
            lpp[len++] = CAYENNE_LPP_TEMPERATURE;
            lpp[len++] = temperature_val >> 8;
            lpp[len++] = temperature_val;
            lpp[len++] = 3;
            lpp[len++] = CAYENNE_LPP_ANALOG_INPUT;
            lpp[len++] = battery_val >> 8;
            lpp[len++] = battery_val;

            payload = lpp;
            length = len;
        } else {
            //native payload
            native_payload_t native_payload;
            native_payload.humidity = humidity * 100;
            native_payload.temperature = temperature * 100;
            native_payload.battery = battery;

            payload = (uint8_t*) &native_payload;
            length = sizeof(native_payload_t);
        }

        lora_send((uint8_t*) payload, length);
    }
}

void environmental_deinit()
{
}
