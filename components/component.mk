COMPONENT_SRCDIRS := lmic/src/lmic lmic/src/aes
COMPONENT_ADD_INCLUDEDIRS := lmic/src/lmic
CFLAGS += -Wno-error=maybe-uninitialized -Wno-error=unused-value -DARDUINO_LMIC_PROJECT_CONFIG_H=../../main/lmic_config.h