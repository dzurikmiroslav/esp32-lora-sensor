#ifndef LMIC_CONFIG_H_
#define LMIC_CONFIG_H_

#define CFG_eu868 1
#define CFG_sx1276_radio 1

#define US_PER_OSTICK_EXPONENT 4
#define US_PER_OSTICK (1 << US_PER_OSTICK_EXPONENT)
#define OSTICKS_PER_SEC (1000000 / US_PER_OSTICK)

#define USE_ORIGINAL_AES

#if LMIC_DEBUG_LEVEL > 0 || LMIC_X_DEBUG_LEVEL > 0
#include <stdio.h>
#endif

#define DISABLE_PING

#define DISABLE_BEACONS

#endif /* LMIC_CONFIG_H_ */
