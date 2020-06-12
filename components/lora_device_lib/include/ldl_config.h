#ifndef LDL_CONFIG_H_
#define LDL_CONFIG_H_

#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_log.h"

#define LDL_ENABLE_SX1276
#define LDL_ENABLE_EU_863_870

#define LDL_DISABLE_POINTONE
//#define LDL_DISABLE_SESSION_UPDATE
//#define LDL_ENABLE_STATIC_RX_BUFFER
//#define LDL_DISABLE_CHECK
//#define LDL_DISABLE_DEVICE_TIME
//#define LDL_DISABLE_FULL_CHANNEL_CONFIG
//#define LDL_DISABLE_CMD_DL_CHANNEL

#define LDL_ERROR(APP,...) do{ESP_LOGE("LDL", __VA_ARGS__);}while(0);
#define LDL_DEBUG(APP,...) do{ESP_LOGI("LDL", __VA_ARGS__);}while(0);
#define LDL_INFO(APP,...) do{ESP_LOGI("LDL", __VA_ARGS__);}while(0);
#define LDL_ASSERT(x) assert((x));
#define LDL_PEDANTIC(x) assert((x));

extern portMUX_TYPE ldl_mutex;

#define LDL_SYSTEM_ENTER_CRITICAL(APP) portENTER_CRITICAL(&ldl_mutex);
#define LDL_SYSTEM_LEAVE_CRITICAL(APP) portEXIT_CRITICAL(&ldl_mutex);

#endif /* LDL_CONFIG_H_ */
