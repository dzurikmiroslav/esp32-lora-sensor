#ifndef LORA_H_
#define LORA_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

void lora_init();

bool lora_is_joined();

bool lora_join();

void lora_start(QueueHandle_t join_queue);

void lora_stop();

void lora_deinit();

void lora_send(const void *payload, uint8_t len);

#endif /* LORA_H_ */
