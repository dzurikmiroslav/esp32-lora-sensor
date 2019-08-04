#ifndef PROFILE_PROFILE_H_
#define PROFILE_PROFILE_H_

#include <stdbool.h>

typedef enum {
    PROFILE_ENVIRONMENTAL,
    PROFILE_SOIL_MOISTURE
} profile_t;

typedef void (*profile_init_cb_t)();

typedef void (*profile_execute_cb_t)(bool lora, bool ble);

typedef void (*profile_deinit_cb_t)();

typedef struct {
    profile_init_cb_t init;
    profile_execute_cb_t execute;
    profile_deinit_cb_t deinit;
} profile_callbacks_t;

profile_callbacks_t profile_get_callbacks(profile_t profile);

#endif /* PROFILE_PROFILE_H_ */
