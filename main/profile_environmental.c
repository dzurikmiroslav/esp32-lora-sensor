#include "profile_environmental.h"

#include "nvs.h"

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
}__attribute__((packed)) native_payload_t;

void environmental_init()
{
}

void environmental_execute(bool lora, bool ble)
{
    uint8_t battery = battery_measure();
    float humidity, temperature;
    sensor_read(&humidity, &temperature);

    if (ble) {
        ble_set_battery(battery);
        ble_set_enviromental(humidity, temperature);
    }

    if (lora) {
        uint8_t format = 0;
        nvs_get_u8(storage, STORAGE_KEY_PAYL_FMT, &format);

        if (format == 1) {
            //cayenne lpp payload
            uint8_t humidity_val = humidity * 2;
            int16_t temperature_val = temperature * 10;
            int16_t battery_val = battery * 100;

            uint8_t len = 0;
            uint8_t payload[15];
            payload[len++] = 1;
            payload[len++] = CAYENNE_LPP_RELATIVE_HUMIDITY;
            payload[len++] = humidity_val;
            payload[len++] = 2;
            payload[len++] = CAYENNE_LPP_TEMPERATURE;
            payload[len++] = temperature_val >> 8;
            payload[len++] = temperature_val;
            payload[len++] = 3;
            payload[len++] = CAYENNE_LPP_ANALOG_INPUT;
            payload[len++] = battery_val >> 8;
            payload[len++] = battery_val;

            lora_send(payload, len);
        } else {
            //native payload
            native_payload_t payload;
            payload.humidity = humidity * 100;
            payload.temperature = temperature * 100;
            payload.battery = battery;

            lora_send((uint8_t*) &payload, sizeof(native_payload_t));
        }
    }
}

void environmental_deinit()
{
}
