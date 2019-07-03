#ifndef I2C_H_
#define I2C_H_

#include <esp_err.h>

void i2c_init();

int8_t i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);

int8_t i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len);

#endif /* I2C_H_ */
