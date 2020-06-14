#ifndef BLE_H_
#define BLE_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
    BLE_EVENT_CONNECT, BLE_EVENT_DISCONNECT, BLE_EVENT_PERIOD_UPDATE, BLE_EVENT_LORA_UPDATED
} ble_event_t;

extern QueueHandle_t ble_event_queue;

void ble_init();

bool ble_has_context();

void ble_deinit();

void ble_set_battery(uint8_t battery);

void ble_set_enviromental(float humidity, float temperature);

#endif /* BLE_H_ */
