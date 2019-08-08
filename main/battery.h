#ifndef BATTERY_H_
#define BATTERY_H_

#include <stdint.h>

void battery_measure_init();

/**
 * Return battery value between [0-100]
 */
uint8_t battery_measure();

#endif /* BATTERY_H_ */
