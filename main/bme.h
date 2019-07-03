#ifndef BME_H_
#define BME_H_

typedef struct
{
    float pressure;
    float temperature;
    float humidity;
} bme_data_t;

void bme_init();

bme_data_t bme_read();

#endif /* BME_H_ */
