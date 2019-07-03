COMPONENT_SRCDIRS := lmic/src/lmic lmic/src/aes bme280
COMPONENT_ADD_INCLUDEDIRS := lmic/src/lmic bme280
CFLAGS += -Wno-error=maybe-uninitialized -Wno-error=unused-value -DARDUINO_LMIC_PROJECT_CONFIG_H=../../main/lmic_config.h