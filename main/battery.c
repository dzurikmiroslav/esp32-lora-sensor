#include "battery.h"

#include <math.h>
#include "esp_log.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#include "peripherals.h"

#define ADC_ATTEN   ADC_ATTEN_DB_11
#define ADC_WIDTH   ADC_WIDTH_BIT_12
#define MAX_VCC     4200
#define MIN_VCC     1650

static const char *TAG = "battery";

static esp_adc_cal_characteristics_t adc_char;

void battery_measure_init()
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(VBAT_ADC1_CHN, ADC_ATTEN);

    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH, 1100, &adc_char);
}

uint8_t battery_measure()
{
    int adc_reading = adc1_get_raw(VBAT_ADC1_CHN);
    uint32_t vcc = 2 * esp_adc_cal_raw_to_voltage(adc_reading, &adc_char);

    float v = ((vcc > MAX_VCC ? MAX_VCC : vcc) - MIN_VCC);
    if (v < 0) {
        v = 0;
    }
    uint8_t value = v / (float) (MAX_VCC - MIN_VCC) * 100;

    ESP_LOGI(TAG, "Measure %d%% (%dmV)", value, vcc);

    return value;
}
