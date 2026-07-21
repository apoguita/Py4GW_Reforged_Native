#include "base/error_handling.h"

#include "listeners/signet_capture_listener.h"

#include "GW/common/constants/skills.h"
#include "GW/common/stoc.h"
#include "GW/stoc/stoc.h"

#include <algorithm>

namespace PY4GW::listeners {

namespace {

namespace StoCPacket = GW::Packet::StoC;

// Clamp the duplicate count for Signet of Capture to 10. Templated over the two
// wire variants (after / pre map load) which share the {skill_id, count} shape.
template <typename T>
void ClampSignetCount(T* pak) {
    if (static_cast<GW::Constants::SkillID>(pak->skill_id) == GW::Constants::SkillID::Signet_of_Capture) {
        pak->count = std::min<uint32_t>(pak->count, 10);
    }
}

// Legacy registered these before the default window handler (altitude -0x3000)
// so the mutated count is what the skills window ends up displaying.
constexpr int kSignetAltitude = -0x3000;

}  // namespace

void SignetOfCaptureLimitListener::Install() {
    GW::StoC::RegisterPacketCallback<StoCPacket::UpdateSkillCountAfterMapLoad>(
        &after_map_load_entry_,
        [](PY4GW::HookStatus*, StoCPacket::UpdateSkillCountAfterMapLoad* pak) {
            ClampSignetCount(pak);
        },
        kSignetAltitude);

    GW::StoC::RegisterPacketCallback<StoCPacket::UpdateSkillCountPreMapLoad>(
        &pre_map_load_entry_,
        [](PY4GW::HookStatus*, StoCPacket::UpdateSkillCountPreMapLoad* pak) {
            ClampSignetCount(pak);
        },
        kSignetAltitude);
}

void SignetOfCaptureLimitListener::Uninstall() {
    GW::StoC::RemoveCallback<StoCPacket::UpdateSkillCountAfterMapLoad>(&after_map_load_entry_);
    GW::StoC::RemoveCallback<StoCPacket::UpdateSkillCountPreMapLoad>(&pre_map_load_entry_);
}

SignetOfCaptureLimitListener& SignetOfCaptureLimit() {
    static SignetOfCaptureLimitListener instance;
    return instance;
}

}  // namespace PY4GW::listeners
