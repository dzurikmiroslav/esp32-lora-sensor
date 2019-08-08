#include "profile_environmental.h"

#include "nvs.h"

#include "peripherals.h"
#include "battery.h"
#include "bme.h"
#include "ble.h"
#include "lora.h"
#include "storage_key.h"
#include "cayenne.h"

typedef struct
{
    uint16_t humidity;
    int16_t temperature;
    uint32_t pressure;
    uint8_t battery;
}__attribute__((packed)) native_payload_t;

void environmental_init()
{
}

void environmental_execute(bool lora, bool ble)
{
    uint8_t battery = battery_measure();
    bme_data_t bme_data = bme_read();

    if (ble) {
        ble_set_battery(battery);
        ble_set_enviromental(bme_data.humidity, bme_data.temperature, bme_data.pressure);
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
            payload[len++] = battery;

            lora_send(payload, len);
        } else {
            //native payload
            native_payload_t payload;
            payload.humidity = bme_data.humidity * 100;
            payload.temperature = bme_data.temperature * 100;
            payload.pressure = bme_data.pressure * 100;
            payload.battery = battery;

            lora_send((uint8_t*) &payload, sizeof(native_payload_t));
        }
    }
}

void environmental_deinit()
{
}
