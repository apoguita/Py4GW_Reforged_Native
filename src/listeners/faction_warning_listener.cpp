#include "base/error_handling.h"

#include "listeners/faction_warning_listener.h"

#include "base/logger.h"

#include "GW/common/constants/constants.h"
#include "GW/common/constants/maps.h"
#include "GW/context/context.h"
#include "GW/context/world.h"
#include "GW/map/map.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace PY4GW::listeners {

namespace {

using GW::Constants::MapID;

// Luxon/Kurzick challenge missions and elite-area outposts (as in the legacy
// module). Faction is earned as the opposing type in each.
constexpr std::array kLuxonMaps = {
    MapID::The_Deep, MapID::The_Jade_Quarry_Luxon_outpost, MapID::Fort_Aspenwood_Luxon_outpost,
    MapID::Zos_Shivros_Channel, MapID::The_Aurios_Mines};
constexpr std::array kKurzickMaps = {
    MapID::Urgozs_Warren, MapID::The_Jade_Quarry_Kurzick_outpost, MapID::Fort_Aspenwood_Kurzick_outpost,
    MapID::Altrumm_Ruins, MapID::Amatz_Basin};

bool Contains(const auto& maps, MapID id) {
    return std::find(maps.begin(), maps.end(), id) != maps.end();
}

}  // namespace

void FactionWarningListener::SetWarnPercent(int percent) {
    warn_percent_ = std::clamp(percent, 0, 100);
}

void FactionWarningListener::Update(float) {
    if (GW::map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        faction_checked_ = false;  // Loading or explorable area.
        return;
    }
    if (faction_checked_) {
        return;  // Already checked this outpost visit.
    }
    faction_checked_ = true;

    const GW::Context::WorldContext* world = GW::Context::GetWorldContext();
    if (!world || !world->max_luxon || !world->total_earned_kurzick) {
        faction_checked_ = false;  // World context not populated yet; retry next tick.
        return;
    }

    const MapID map_id = GW::map::GetMapID();
    const uint32_t* current;
    const uint32_t* max;
    const uint32_t* other_current;
    const char* name;
    const char* other_name;

    if (Contains(kLuxonMaps, map_id)) {
        current = &world->current_luxon;
        max = &world->max_luxon;
        other_current = &world->current_kurzick;
        name = "Luxon";
        other_name = "Kurzick";
    }
    else if (Contains(kKurzickMaps, map_id)) {
        current = &world->current_kurzick;
        max = &world->max_kurzick;
        other_current = &world->current_luxon;
        name = "Kurzick";
        other_name = "Luxon";
    }
    else {
        return;  // Not a faction outpost.
    }

    char buffer[128];
    const float pct = 100.0f * static_cast<float>(*current) / static_cast<float>(*max);
    if (pct >= static_cast<float>(warn_percent_)) {
        std::snprintf(buffer, sizeof(buffer), "%s faction earned is %u of %u", name, *current, *max);
        Logger::Instance().LogWarning(buffer);
    }
    else if (*other_current > 4999 && *other_current > *current) {
        std::snprintf(buffer, sizeof(buffer), "%s faction earned is greater than %s", other_name, name);
        Logger::Instance().LogWarning(buffer);
    }
}

FactionWarningListener& FactionWarning() {
    static FactionWarningListener instance;
    return instance;
}

}  // namespace PY4GW::listeners
