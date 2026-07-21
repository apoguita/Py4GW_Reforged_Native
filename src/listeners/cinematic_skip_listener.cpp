#include "base/error_handling.h"

#include "listeners/cinematic_skip_listener.h"

#include "GW/common/stoc.h"
#include "GW/map/map.h"
#include "GW/stoc/stoc.h"

namespace PY4GW::listeners {

namespace {
namespace StoCPacket = GW::Packet::StoC;
}

void CinematicSkipListener::Install() {
    GW::StoC::RegisterPacketCallback<StoCPacket::CinematicPlay>(
        &cinematic_play_entry_,
        [](PY4GW::HookStatus*, StoCPacket::CinematicPlay* pak) {
            if (pak->play) {
                GW::map::SkipCinematic();
            }
        });
}

void CinematicSkipListener::Uninstall() {
    GW::StoC::RemoveCallback<StoCPacket::CinematicPlay>(&cinematic_play_entry_);
}

CinematicSkipListener& CinematicSkip() {
    static CinematicSkipListener instance;
    return instance;
}

}  // namespace PY4GW::listeners
