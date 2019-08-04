#ifndef ENVIRONMENTAL_H_
#define ENVIRONMENTAL_H_

#include <stdbool.h>

void environmental_init();

void environmental_execute(bool lora, bool ble);

void environmental_deinit();

#endif /* ENVIRONMENTAL_H_ */
