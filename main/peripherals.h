#ifndef GPIO_PINS_H_
#define GPIO_PINS_H_

#include "driver/gpio.h"
#include "driver/adc.h"

#define SPI_SCLK        GPIO_NUM_18
#define SPI_MISO        GPIO_NUM_19
#define SPI_MOSI        GPIO_NUM_23
#define SPI_RFM_NSS     GPIO_NUM_5
#define SPI_RFM_RESET   GPIO_NUM_17
#define SPI_RFM_DIO0    GPIO_NUM_4
#define SPI_RFM_DIO1    GPIO_NUM_16

#define BUTTON_BLE      GPIO_NUM_0
#define LED_BLE         GPIO_NUM_2
#define LED_LORA        GPIO_NUM_15
#define LED_ERR         GPIO_NUM_13

#define I2C_SDA         GPIO_NUM_12
#define I2C_SCL         GPIO_NUM_14

#define EXT_0           GPIO_NUM_26
#define EXT_1           GPIO_NUM_25
#define EXT_2           GPIO_NUM_33
#define EXT_3           GPIO_NUM_35
#define EXT_NSW         GPIO_NUM_27

#define EXT_0_ADC2_CHN  ADC2_CHANNEL_9
#define EXT_1_ADC2_CHN  ADC2_CHANNEL_8
#define EXT_2_ADC1_CHN  ADC1_CHANNEL_5
#define EXT_3_ADC1_CHN  ADC1_CHANNEL_7

void spi_init();

void i2c_init();

int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);

int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);


#endif /* GPIO_PINS_H_ */
