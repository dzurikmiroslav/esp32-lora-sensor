set(COMPONENT_SRCS "main.c"
                   "battery.c"
                   "ble.c"
                   "lora.c"
                   "peripherals.c"
                   "profile_environmental.c"
                   "profile_soil_moisture.c"
                   "sensor.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()