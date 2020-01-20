#include "profile.h"
#include "profile_environmental.h"
#include "profile_soil_moisture.h"

profile_callbacks_t profile_get_callbacks(profile_t profile)
{
    profile_callbacks_t cbs;

    switch (profile) {
        case PROFILE_SOIL_MOISTURE:
            cbs.init = soil_mousture_init;
            cbs.execute = soil_mousture_execute;
            cbs.deinit = soil_mousture_deinit;
            break;
        default:
            cbs.init = environmental_init;
            cbs.execute = environmental_execute;
            cbs.deinit = environmental_deinit;
    }

    return cbs;
}
