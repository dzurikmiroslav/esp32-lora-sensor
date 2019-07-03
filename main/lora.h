#ifndef LORA_H_
#define LORA_H_

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"

#define LORA_UNUSED_PIN 0xff

extern QueueHandle_t lora_join_queue;

void lora_init(spi_host_device_t spi_host, uint8_t nss, uint8_t rxtx, uint8_t rst, uint8_t dio0, uint8_t dio1);

void lora_start_joining();

void lora_deinit();

void lora_send(float humidity, float temperature, float pressure, uint8_t battery);

void lora_get_counters(uint32_t* seqno_up, uint32_t* seqno_dw);

void lora_get_session(uint32_t *dev_addr, uint8_t *nwk_key, uint8_t *art_key);

void lora_set_session(uint32_t dev_addr, uint8_t *nwk_key, uint8_t *art_key, uint32_t seqno_up, uint32_t seqno_dw);

#endif /* LORA_H_ */
