#ifndef CAYENE_H_
#define CAYENE_H_

#define CAYENNE_LPP_DIGITAL_INPUT            (0U)   /* 1 byte */
#define CAYENNE_LPP_DIGITAL_OUTPUT           (1U)   /* 1 byte */
#define CAYENNE_LPP_ANALOG_INPUT             (2U)   /* 2 bytes, 0.01 signed */
#define CAYENNE_LPP_ANALOG_OUTPUT            (3U)   /* 2 bytes, 0.01 signed */
#define CAYENNE_LPP_LUMINOSITY               (101U) /* 2 bytes, 1 lux unsigned */
#define CAYENNE_LPP_PRESENCE                 (102U) /* 1 byte, 1 */
#define CAYENNE_LPP_TEMPERATURE              (103U) /* 2 bytes, 0.1°C signed */
#define CAYENNE_LPP_RELATIVE_HUMIDITY        (104U) /* 1 byte, 0.5% unsigned */
#define CAYENNE_LPP_ACCELEROMETER            (113U) /* 2 bytes per axis, 0.001G */
#define CAYENNE_LPP_BAROMETRIC_PRESSURE      (115U) /* 2 bytes 0.1 hPa Unsigned */
#define CAYENNE_LPP_GYROMETER                (134U) /* 2 bytes per axis, 0.01 °/s */
#define CAYENNE_LPP_GPS                      (136U) /* 3 byte lon/lat 0.0001 °, 3 bytes alt 0.01 meter */

#endif /* CAYENE_H_ */
