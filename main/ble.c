#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "nvs.h"

#include "ble.h"
#include "storage_key.h"

static const char *TAG = "ble";

#define DEVICE_NAME "ESP32 LoRa Sensor"
#define PROFILE_APP_ID 0

#define ADV_CONFIG_FLAG (1 << 0)
#define SCAN_RSP_CONFIG_FLAG (1 << 1)

static uint8_t adv_config_done = 0;

QueueHandle_t ble_event_queue;

// @formatter:off
static uint8_t service_uuid[32] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xC8, 0x00, 0x00, 0x00,
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x18, 0x0F, 0x00, 0x00
};

static esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(service_uuid),
        .p_service_uuid = service_uuid,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_data_t scan_rsp_data = {
        .set_scan_rsp = true,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(service_uuid),
        .p_service_uuid = service_uuid,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x0800,
        .adv_int_max = 0x0800,
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

enum
{
    LORA_IDX_SVC,

    LORA_IDX_CHAR_PERIOD,
    LORA_IDX_CHAR_VAL_PERIOD,
    LORA_IDX_CHAR_CFG_PERIOD,

    LORA_IDX_CHAR_JOIN_EUI,
    LORA_IDX_CHAR_VAL_JOIN_EUI,
    LORA_IDX_CHAR_CFG_JOIN_EUI,

    LORA_IDX_CHAR_DEV_EUI,
    LORA_IDX_CHAR_VAL_DEV_EUI,
    LORA_IDX_CHAR_CFG_DEV_EUI,

    LORA_IDX_CHAR_APP_KEY,
    LORA_IDX_CHAR_VAL_APP_KEY,
    LORA_IDX_CHAR_CFG_APP_KEY,

    LORA_IDX_CHAR_NWK_KEY,
    LORA_IDX_CHAR_VAL_NWK_KEY,
    LORA_IDX_CHAR_CFG_NWK_KEY,

    LORA_IDX_CHAR_PAYL_FMT,
    LORA_IDX_CHAR_VAL_PAYL_FMT,
    LORA_IDX_CHAR_CFG_PAYL_FMT,

    LORA_IDX_CHAR_CONFM,
    LORA_IDX_CHAR_VAL_CONFM,
    LORA_IDX_CHAR_CFG_CONFM,

    LORA_IDX_NB
};

enum
{
    ENV_SENS_IDX_SVC,

    ENV_SENS_IDX_CHAR_HUM,
    ENV_SENS_IDX_CHAR_VAL_HUM,
    ENV_SENS_IDX_CHAR_CFG_HUM,

    ENV_SENS_IDX_CHAR_TEMP,
    ENV_SENS_IDX_CHAR_VAL_TEMP,
    ENV_SENS_IDX_CHAR_CFG_TEMP,

    ENV_SENS_IDX_NB
};

enum
{
   BAT_SERV_IDX_SVC,

   BAT_SERV_IDX_CHAR_BAT_LVL,
   BAT_SERV_IDX_CHAR_VAL_BAT_LVL,
   BAT_SERV_IDX_CHAR_CFG_BAT_LVL,

   BAT_SERV_IDX_NB
};

static const uint16_t GATTS_SERVICE_UUID_LORA       = 0xC800;
static const uint16_t GATTS_SERVICE_UUID_ENV_SENS   = 0x181A;
static const uint16_t GATTS_SERVICE_UUID_BAT_SERV   = 0x180F;
static const uint16_t GATTS_CHAR_UUID_PERIOD        = 0xC901;
static const uint16_t GATTS_CHAR_UUID_JOIN_EUI      = 0xC902;
static const uint16_t GATTS_CHAR_UUID_DEV_EUI       = 0xC903;
static const uint16_t GATTS_CHAR_UUID_APP_KEY       = 0xC904;
static const uint16_t GATTS_CHAR_UUID_NWK_KEY       = 0xC905;
//static const uint16_t GATTS_CHAR_UUID_PROFILE       = 0xC906;
static const uint16_t GATTS_CHAR_UUID_PAYL_FMT      = 0xC907;
static const uint16_t GATTS_CHAR_UUID_CONFM         = 0xC908;
static const uint16_t GATTS_CHAR_UUID_HUM           = 0x2A6F;
static const uint16_t GATTS_CHAR_UUID_TEMP          = 0x2A6E;
static const uint16_t GATTS_CHAR_UUID_BAT_LVL       = 0x2A19;

static const uint16_t primary_service_uuid = ESP_GATT_UUID_PRI_SERVICE;
static const uint16_t character_declaration_uuid = ESP_GATT_UUID_CHAR_DECLARE;
static const uint16_t character_client_config_uuid = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
static const uint8_t char_prop_read = ESP_GATT_CHAR_PROP_BIT_READ;
static const uint8_t char_prop_read_write = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE;
static const uint8_t char_prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static uint8_t period_ccc[2] = {0x00, 0x00};

static uint8_t dev_eui_ccc[2] = {0x00, 0x00};

static uint8_t app_eui_ccc[2] = {0x00, 0x00};

static uint8_t dev_key_ccc[2] = {0x00, 0x00};

static uint8_t payl_fmt_ccc[2] = {0x00, 0x00};

static uint8_t confm_ccc[2] = {0x00, 0x00};

static uint8_t bat_lvl_val = 0;
static uint8_t bat_lvl_ccc[2] = {0x00, 0x00};

static uint16_t hum_val = 0;
static uint8_t hum_ccc[2] = {0x00, 0x00};

static int16_t temp_val = 0;
static uint8_t temp_ccc[2] = {0x00, 0x00};

static const esp_gatts_attr_db_t gatt_lora_db[LORA_IDX_NB] = {
    [LORA_IDX_SVC] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&GATTS_SERVICE_UUID_LORA}},

    [LORA_IDX_CHAR_PERIOD] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_PERIOD] =
        {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_PERIOD, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint16_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_PERIOD] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)period_ccc}},

    [LORA_IDX_CHAR_JOIN_EUI] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_JOIN_EUI] =
         {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_JOIN_EUI, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 8 * sizeof(uint8_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_JOIN_EUI] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)app_eui_ccc}},

    [LORA_IDX_CHAR_DEV_EUI] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_DEV_EUI] =
         {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_DEV_EUI, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 8 * sizeof(uint8_t),0, NULL}},
    [LORA_IDX_CHAR_CFG_DEV_EUI] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)dev_eui_ccc}},

    [LORA_IDX_CHAR_APP_KEY] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_APP_KEY] =
        {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_APP_KEY, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 16 * sizeof(uint8_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_APP_KEY] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)dev_key_ccc}},

    [LORA_IDX_CHAR_NWK_KEY] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_NWK_KEY] =
        {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_NWK_KEY, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 16 * sizeof(uint8_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_NWK_KEY] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)dev_key_ccc}},

    [LORA_IDX_CHAR_PAYL_FMT] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_PAYL_FMT] =
         {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_PAYL_FMT, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint8_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_PAYL_FMT] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)payl_fmt_ccc}},

    [LORA_IDX_CHAR_CONFM] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_write}},
    [LORA_IDX_CHAR_VAL_CONFM] =
         {{ESP_GATT_RSP_BY_APP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_CONFM, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, sizeof(uint8_t), 0, NULL}},
    [LORA_IDX_CHAR_CFG_CONFM] =
         {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)confm_ccc}},

};

static const esp_gatts_attr_db_t gatt_env_sens_db[ENV_SENS_IDX_NB] = {
    [ENV_SENS_IDX_SVC] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&GATTS_SERVICE_UUID_ENV_SENS}},

    [ENV_SENS_IDX_CHAR_HUM] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_notify}},
    [ENV_SENS_IDX_CHAR_VAL_HUM] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_HUM, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&hum_val}},
    [ENV_SENS_IDX_CHAR_CFG_HUM] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)hum_ccc}},

    [ENV_SENS_IDX_CHAR_TEMP] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read_notify}},
    [ENV_SENS_IDX_CHAR_VAL_TEMP] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_TEMP, ESP_GATT_PERM_READ, sizeof(int16_t), sizeof(int16_t), (uint8_t *)&temp_val}},
    [ENV_SENS_IDX_CHAR_CFG_TEMP] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)temp_ccc}},
};

static const esp_gatts_attr_db_t gatt_bat_serv_db[BAT_SERV_IDX_NB] = {
    [BAT_SERV_IDX_SVC] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid, ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&GATTS_SERVICE_UUID_BAT_SERV}},

    [BAT_SERV_IDX_CHAR_BAT_LVL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_declaration_uuid, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&char_prop_read}},
    [BAT_SERV_IDX_CHAR_VAL_BAT_LVL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&GATTS_CHAR_UUID_BAT_LVL, ESP_GATT_PERM_READ, sizeof(uint8_t), sizeof(uint8_t), (uint8_t *)&bat_lvl_val}},
    [BAT_SERV_IDX_CHAR_CFG_BAT_LVL] =
        {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t *)&character_client_config_uuid, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2 * sizeof(uint8_t), 2 * sizeof(uint8_t), (uint8_t *)bat_lvl_ccc}},
};
// @formatter:on

uint16_t lora_handle_table[LORA_IDX_NB];
uint16_t bat_serv_handle_table[BAT_SERV_IDX_NB];
uint16_t env_sens_handle_table[ENV_SENS_IDX_NB];

static esp_gatt_if_t ble_gatts_if = 0;
static uint16_t ble_connection_id = 0;
static bool ble_has_connection;

void gatts_read(esp_gatt_if_t gatts_if, struct gatts_read_evt_param read)
{
    esp_gatt_rsp_t rsp;
    rsp.attr_value.handle = read.handle;
    size_t length = 0;
    rsp.attr_value.len = 0;
    memset(rsp.attr_value.value, 0, sizeof(uint8_t) * 32); // no attribute longer than 32

    if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_PERIOD]) {
        rsp.attr_value.len = sizeof(uint16_t);
        nvs_get_u16(storage, STORAGE_KEY_PERIOD, (uint16_t*) rsp.attr_value.value);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_JOIN_EUI]) {
        rsp.attr_value.len = length = sizeof(uint8_t) * 8;
        nvs_get_blob(storage, STORAGE_KEY_LORA_JOIN_EUI, (void*) rsp.attr_value.value, &length);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_DEV_EUI]) {
        rsp.attr_value.len = length = sizeof(uint8_t) * 8;
        nvs_get_blob(storage, STORAGE_KEY_LORA_DEV_EUI, (void*) rsp.attr_value.value, &length);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_APP_KEY]) {
        rsp.attr_value.len = length = sizeof(uint8_t) * 16;
        nvs_get_blob(storage, STORAGE_KEY_LORA_APP_KEY, (void*) rsp.attr_value.value, &length);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_NWK_KEY]) {
        rsp.attr_value.len = length = sizeof(uint8_t) * 16;
        nvs_get_blob(storage, STORAGE_KEY_LORA_NWK_KEY, (void*) rsp.attr_value.value, &length);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_PAYL_FMT]) {
        rsp.attr_value.len = sizeof(uint8_t);
        nvs_get_u8(storage, STORAGE_KEY_PAYL_FMT, (uint8_t*) rsp.attr_value.value);
    } else if (read.handle == lora_handle_table[LORA_IDX_CHAR_VAL_CONFM]) {
        rsp.attr_value.len = sizeof(uint8_t);
        nvs_get_u8(storage, STORAGE_KEY_CONFM, (uint8_t*) rsp.attr_value.value);
    }

    if (rsp.attr_value.len > 0) {
        esp_ble_gatts_send_response(gatts_if, read.conn_id, read.trans_id, ESP_GATT_OK, &rsp);
    }
}

void gatts_write(esp_gatt_if_t gatts_if, struct gatts_write_evt_param write)
{
    ble_event_t ble_event;
    if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_PERIOD]) {
        uint16_t val;
        memcpy(&val, write.value, sizeof(uint16_t));
        nvs_set_u16(storage, STORAGE_KEY_PERIOD, val);
        ble_event = BLE_EVENT_PERIOD_UPDATE;
        xQueueSend(ble_event_queue, &ble_event, 0);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_JOIN_EUI]) {
        nvs_set_blob(storage, STORAGE_KEY_LORA_JOIN_EUI, write.value, 8 * sizeof(uint8_t));
        ble_event = BLE_EVENT_LORA_UPDATED;
        xQueueSend(ble_event_queue, &ble_event, 0);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_DEV_EUI]) {
        nvs_set_blob(storage, STORAGE_KEY_LORA_DEV_EUI, write.value, 8 * sizeof(uint8_t));
        ble_event = BLE_EVENT_LORA_UPDATED;
        xQueueSend(ble_event_queue, &ble_event, 0);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_APP_KEY]) {
        nvs_set_blob(storage, STORAGE_KEY_LORA_APP_KEY, write.value, 16 * sizeof(uint8_t));
        ble_event = BLE_EVENT_LORA_UPDATED;
        xQueueSend(ble_event_queue, &ble_event, 0);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_NWK_KEY]) {
        nvs_set_blob(storage, STORAGE_KEY_LORA_NWK_KEY, write.value, 16 * sizeof(uint8_t));
        ble_event = BLE_EVENT_LORA_UPDATED;
        xQueueSend(ble_event_queue, &ble_event, 0);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_PAYL_FMT]) {
        uint8_t val;
        memcpy(&val, write.value, sizeof(uint8_t));
        nvs_set_u8(storage, STORAGE_KEY_PAYL_FMT, val);
    } else if (write.handle == lora_handle_table[LORA_IDX_CHAR_VAL_CONFM]) {
        uint8_t val;
        memcpy(&val, write.value, sizeof(uint8_t));
        nvs_set_u8(storage, STORAGE_KEY_CONFM, val);
    }

    if (write.need_rsp) {
        if (write.is_prep) {
            esp_gatt_rsp_t gatt_rsp;
            gatt_rsp.attr_value.len = write.len;
            gatt_rsp.attr_value.handle = write.handle;
            gatt_rsp.attr_value.offset = write.offset;
            memcpy(gatt_rsp.attr_value.value, write.value, write.len);
            gatt_rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
            esp_ble_gatts_send_response(gatts_if, write.conn_id, write.trans_id, ESP_GATT_OK, &gatt_rsp);
        } else {
            esp_ble_gatts_send_response(gatts_if, write.conn_id, write.trans_id, ESP_GATT_OK, NULL);
        }
    }
}

void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "Gatts event handler [event: %d]", event);
    ble_event_t ble_event;
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ble_gatts_if = gatts_if;
            esp_ble_gap_set_device_name(DEVICE_NAME);

            esp_ble_gap_config_adv_data(&adv_data);
            adv_config_done |= ADV_CONFIG_FLAG;

            esp_ble_gap_config_adv_data(&scan_rsp_data);
            adv_config_done |= SCAN_RSP_CONFIG_FLAG;

            esp_ble_gatts_create_attr_tab(gatt_lora_db, gatts_if, LORA_IDX_NB, 0);
            esp_ble_gatts_create_attr_tab(gatt_env_sens_db, gatts_if, ENV_SENS_IDX_NB, 0);
            esp_ble_gatts_create_attr_tab(gatt_bat_serv_db, gatts_if, BAT_SERV_IDX_NB, 0);
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.svc_uuid.uuid.uuid16 == GATTS_SERVICE_UUID_LORA) {
                memcpy(lora_handle_table, param->add_attr_tab.handles, sizeof(lora_handle_table));
                esp_ble_gatts_start_service(lora_handle_table[LORA_IDX_SVC]);
            }
            if (param->add_attr_tab.svc_uuid.uuid.uuid16 == GATTS_SERVICE_UUID_ENV_SENS) {
                memcpy(env_sens_handle_table, param->add_attr_tab.handles, sizeof(env_sens_handle_table));
                esp_ble_gatts_start_service(env_sens_handle_table[ENV_SENS_IDX_SVC]);
            }
            if (param->add_attr_tab.svc_uuid.uuid.uuid16 == GATTS_SERVICE_UUID_BAT_SERV) {
                memcpy(bat_serv_handle_table, param->add_attr_tab.handles, sizeof(bat_serv_handle_table));
                esp_ble_gatts_start_service(bat_serv_handle_table[BAT_SERV_IDX_SVC]);
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            ble_connection_id = param->connect.conn_id;
            ble_has_connection = true;
            ble_event = BLE_EVENT_CONNECT;
            xQueueSend(ble_event_queue, &ble_event, 0);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            ble_has_connection = false;
            esp_ble_gap_start_advertising(&adv_params);
            ble_event = BLE_EVENT_DISCONNECT;
            xQueueSend(ble_event_queue, &ble_event, 0);
            break;
        case ESP_GATTS_READ_EVT:
            gatts_read(gatts_if, param->read);
            break;
        case ESP_GATTS_WRITE_EVT:
            gatts_write(gatts_if, param->write);
            break;
        default:
            break;
    }
}

void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    ESP_LOGD(TAG, "Gap event handler [event: %d]", event);
    switch (event) {
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~SCAN_RSP_CONFIG_FLAG);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            adv_config_done &= (~ADV_CONFIG_FLAG);
            if (adv_config_done == 0) {
                esp_ble_gap_start_advertising(&adv_params);
            }
            break;
        default:
            break;
    }
}

void ble_init()
{
    ble_event_queue = xQueueCreate(5, sizeof(ble_event_t));

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT()
    ;

    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));

    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_APP_ID));

    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
    ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12));
}

bool ble_has_context()
{
    return ble_gatts_if != 0;
}

void ble_deinit()
{
    ESP_ERROR_CHECK(esp_ble_gatts_app_unregister(ble_gatts_if));
    ble_gatts_if = 0;

    ESP_ERROR_CHECK(esp_bluedroid_disable());
    ESP_ERROR_CHECK(esp_bluedroid_deinit());

    ESP_ERROR_CHECK(esp_bt_controller_disable());

    ESP_ERROR_CHECK(esp_bt_controller_deinit());

//    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_IDLE));

    vQueueDelete(ble_event_queue);
}

void ble_set_battery(uint8_t battery)
{
    bat_lvl_val = battery;

    esp_ble_gatts_set_attr_value(bat_serv_handle_table[BAT_SERV_IDX_CHAR_BAT_LVL], sizeof(uint8_t),
            (uint8_t*) &bat_lvl_val);

    if (ble_has_connection) {
        esp_ble_gatts_send_indicate(ble_gatts_if, ble_connection_id, bat_serv_handle_table[BAT_SERV_IDX_CHAR_BAT_LVL],
                sizeof(uint8_t), (uint8_t*) &bat_lvl_val, false);
    }
}

void ble_set_enviromental(float humidity, float temperature)
{
    hum_val = humidity * 100;
    temp_val = temperature * 100;

    esp_ble_gatts_set_attr_value(env_sens_handle_table[ENV_SENS_IDX_CHAR_VAL_HUM], sizeof(uint16_t),
            (uint8_t*) &hum_val);
    esp_ble_gatts_set_attr_value(env_sens_handle_table[ENV_SENS_IDX_CHAR_VAL_TEMP], sizeof(int16_t),
            (uint8_t*) &temp_val);
    esp_ble_gatts_set_attr_value(bat_serv_handle_table[BAT_SERV_IDX_CHAR_VAL_BAT_LVL], sizeof(uint8_t),
            (uint8_t*) &bat_lvl_val);

    if (ble_has_connection) {
        esp_ble_gatts_send_indicate(ble_gatts_if, ble_connection_id, env_sens_handle_table[ENV_SENS_IDX_CHAR_VAL_HUM],
                sizeof(uint16_t), (uint8_t*) &hum_val, false);
        esp_ble_gatts_send_indicate(ble_gatts_if, ble_connection_id, env_sens_handle_table[ENV_SENS_IDX_CHAR_VAL_TEMP],
                sizeof(int16_t), (uint8_t*) &temp_val, false);
    }
}
