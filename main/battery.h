#ifndef BATTERY_H_
#define BATTERY_H_

#include <stdint.h>

void battery_measure_init();

void battery_measure(uint8_t *battery, float *battery_voltage);

#endif /* BATTERY_H_ */
