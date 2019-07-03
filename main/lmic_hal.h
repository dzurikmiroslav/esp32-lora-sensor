#ifndef LMIC_HAL_H_
#define LMIC_HAL_H_

#include <stdint.h>

typedef struct lmic_pinmap
{
    spi_host_device_t spi_host;
    uint8_t nss;
    uint8_t rxtx;
    uint8_t rst;
    uint8_t dio0;
    uint8_t dio1;
} lmic_pinmap;

extern lmic_pinmap lmic_pins;

void lmic_hal_enter_critical();

void lmic_hal_leave_critical();

void lmic_hal_wakeup();

void lmic_hal_free();

#endif /* LMIC_HAL_H_ */
