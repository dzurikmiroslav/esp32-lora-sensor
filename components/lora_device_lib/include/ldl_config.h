#ifndef LDL_CONFIG_H_
#define LDL_CONFIG_H_

#include <stdio.h>
#include <assert.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define LDL_ENABLE_SX1276

#ifdef CONFIG_LORA_FREQUENCY_EU_863_870
#define LDL_ENABLE_EU_863_870
#endif /* CONFIG_LORA_FREQUENCY_EU_863_870 */

#ifdef CONFIG_LORA_FREQUENCY_EU_433
#define LDL_ENABLE_EU_433
#endif /* CONFIG_LORA_FREQUENCY_EU_433 */

#ifdef CONFIG_LORA_FREQUENCY_US_902_928
#define LDL_ENABLE_US_902_928
#endif /* CONFIG_LORA_FREQUENCY_US_902_928 */

#ifdef CONFIG_LORA_FREQUENCY_AU_915_928
#define LDL_ENABLE_AU_915_928
#endif /* CONFIG_LORA_FREQUENCY_AU_915_928 */

#ifdef CONFIG_LORA_LORAWAN_VERSION_1_1
#define LDL_DISABLE_POINTONE
#endif /* CONFIG_LORA_LORAWAN_VERSION_1_1 */

#define LDL_ERROR(APP,...) do{ESP_LOGE("LDL", __VA_ARGS__);}while(0);
#define LDL_DEBUG(APP,...) do{ESP_LOGI("LDL", __VA_ARGS__);}while(0);
#define LDL_INFO(APP,...) do{ESP_LOGI("LDL", __VA_ARGS__);}while(0);

extern portMUX_TYPE ldl_mutex;

#define LDL_SYSTEM_ENTER_CRITICAL(APP) portENTER_CRITICAL(&ldl_mutex);
#define LDL_SYSTEM_LEAVE_CRITICAL(APP) portEXIT_CRITICAL(&ldl_mutex);

#endif /* LDL_CONFIG_H_ */
