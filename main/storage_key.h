#ifndef NVS_VARIABLE_H_
#define NVS_VARIABLE_H_

#define STORAGE_NAME "lora"

#define STORAGE_KEY_LORA_JOIN_EUI   "lora_join_eui"
#define STORAGE_KEY_LORA_DEV_EUI    "lora_dev_eui"
#define STORAGE_KEY_LORA_APP_KEY    "lora_app_key"
#define STORAGE_KEY_LORA_NWK_KEY    "lora_nwk_key"
#ifdef CONFIG_LORA_LORAWAN_VERSION_1_1
#define STORAGE_KEY_LORA_DEV_NONCE  "lora_dev_nonce"
#define STORAGE_KEY_LORA_JOIN_NONCE "lora_join_nonce"
#endif /* CONFIG_LORA_LORAWAN_VERSION_1_1 */
#define STORAGE_KEY_PERIOD          "period"
#define STORAGE_KEY_PAYL_FMT        "payl_fmt"
#define STORAGE_KEY_CONFM           "confm"

extern nvs_handle storage;

#endif /* NVS_VARIABLE_H_ */
