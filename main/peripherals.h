#ifndef PERIPHERALS_H_
#define PERIPHERALS_H_

#include "driver/gpio.h"
#include "driver/adc.h"

#define SPI_SCLK        GPIO_NUM_18
#define SPI_MISO        GPIO_NUM_19
#define SPI_MOSI        GPIO_NUM_23
#define RFM_NSS         GPIO_NUM_5
#define RFM_RESET       GPIO_NUM_17
#define RFM_DIO0        GPIO_NUM_4
#define RFM_DIO1        GPIO_NUM_16

#define BUTTON_BLE      GPIO_NUM_0
#define LED_BLE         GPIO_NUM_2
#define LED_LORA        GPIO_NUM_13

#define I2C_SDA         GPIO_NUM_21 //GPIO_NUM_27
#define I2C_SCL         GPIO_NUM_22

#define VBAT_ADC1_CHN   ADC1_CHANNEL_0

typedef enum
{
    LED_ID_BLE, LED_ID_LORA,
} led_id_t;

typedef enum
{
    LED_STATE_ON, LED_STATE_OFF, LED_STATE_DUTY_5, LED_STATE_DUTY_50, LED_STATE_DUTY_90
} led_state_t;

void spi_init();

void i2c_init();

int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);

int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);

void led_init();

void led_deinit();

void led_set_state(led_id_t led_id, led_state_t state);

#endif /* PERIPHERALS_H_ */
