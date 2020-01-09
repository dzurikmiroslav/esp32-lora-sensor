#ifndef SENSOR_H_
#define SENSOR_H_

#define SENSOR_DHT10 1
#define SENSOR_DHT11 2
#define SENSOR_DHT22 3

#define SENSOR_TYPE SENSOR_DHT10

void sensor_init();

void sensor_read(float *humidity, float *temperature);

#endif /* SENSOR_H_ */
