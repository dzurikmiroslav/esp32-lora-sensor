#ifndef SOIL_MOISTURE_H_
#define SOIL_MOISTURE_H_

#include <stdbool.h>

void soil_mousture_init();

void soil_mousture_execute(bool lora, bool ble);

void soil_mousture_deinit();

#endif /* SOIL_MOISTURE_H_ */
