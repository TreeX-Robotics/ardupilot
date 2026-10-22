#include "Copter.h"

#if MODE_GUIDED_ENABLED

// Report configuration once at startup; this does not indicate control execution.
void ModeGuided::report_loaded_options()
{
#if HAL_GCS_ENABLED
    static const struct {
        Option option;
        const char *name;
    } flags[] = {
        {Option::AllowArmingFromTX, "AllowArmingFromTX"},
        {Option::IgnorePilotYaw, "IgnorePilotYaw"},
        {Option::SetAttitudeTarget_ThrustAsThrust, "SetAttitudeTarget_ThrustAsThrust"},
        {Option::DoNotStabilizePositionXY, "DoNotStabilizePositionXY"},
        {Option::DoNotStabilizeVelocityXY, "DoNotStabilizeVelocityXY"},
        {Option::WPNavUsedForPosControl, "WPNavUsedForPosControl"},
        {Option::AllowWeatherVaning, "AllowWeatherVaning"},
        {Option::DoNotStabilizePositionZ, "DoNotStabilizePositionZ"},
        {Option::DoNotStabilizeHeading, "DoNotStabilizeHeading"},
    };

    const uint32_t options = static_cast<uint32_t>(g2.guided_options.get());
    GCS_SEND_TEXT(MAV_SEVERITY_INFO, "GUID_OPTIONS loaded=%lu", static_cast<unsigned long>(options));
    if (options == 0) {
        GCS_SEND_TEXT(MAV_SEVERITY_INFO, "GUID_OPTIONS: no flags enabled");
        return;
    }

    for (uint8_t bit = 0; bit < 32; bit++) {
        const uint32_t mask = uint32_t{1} << bit;
        if ((options & mask) == 0) {
            continue;
        }
        const char *name = "unknown";
        for (const auto &flag : flags) {
            if (static_cast<uint32_t>(flag.option) == mask) {
                name = flag.name;
                break;
            }
        }
        GCS_SEND_TEXT(MAV_SEVERITY_INFO, "GUID bit%u: %s", unsigned(bit), name);
    }
#endif // HAL_GCS_ENABLED
}

#endif // MODE_GUIDED_ENABLED
