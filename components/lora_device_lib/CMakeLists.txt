set(srcs
    "lora_device_lib/src/ldl_aes.c"
    "lora_device_lib/src/ldl_cmac.c"
    "lora_device_lib/src/ldl_ctr.c"
    "lora_device_lib/src/ldl_frame.c"
    "lora_device_lib/src/ldl_mac_commands.c"
    "lora_device_lib/src/ldl_mac.c"
    "lora_device_lib/src/ldl_ops.c"
    "lora_device_lib/src/ldl_radio.c"
    "lora_device_lib/src/ldl_region.c"
    "lora_device_lib/src/ldl_sm.c"
    "lora_device_lib/src/ldl_stream.c"
    "lora_device_lib/src/ldl_system.c"
    "src/ldl_config.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS "lora_device_lib/include" "include"
                    REQUIRES log freertos)

target_compile_definitions(${COMPONENT_LIB} PUBLIC -DLDL_TARGET_INCLUDE="ldl_config.h")