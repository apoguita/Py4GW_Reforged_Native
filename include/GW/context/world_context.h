#pragma once

#include "base/error_handling.h"

#include "GW/context/skill.h"
#include "GW/context/gw_array.h"

#include <cstddef>
#include <cstdint>
#include <windows.h>

namespace gw::context {

using ItemID = uint32_t;
using MerchItemArray = gw::GwArray<ItemID>;

struct PlayerControlledCharacter {
    uint8_t pad_0000[0x14];
    uint32_t agent_id;
};

struct WorldContext {
    void* account_info;
    gw::GwArray<wchar_t> message_buffer;
    gw::GwArray<wchar_t> dialog_buffer;
    MerchItemArray merch_items;
    MerchItemArray merch_items_secondary;
    uint8_t pad_0044_to_0508[0x4C4];
    AgentEffectsArray party_effects;
    uint8_t pad_0518_to_067c[0x164];
    uint32_t player_number;
    PlayerControlledCharacter* player_controlled_character;
};

static_assert(offsetof(WorldContext, merch_items) == 0x24, "WorldContext::merch_items offset mismatch");
static_assert(offsetof(WorldContext, party_effects) == 0x508, "WorldContext::party_effects offset mismatch");
static_assert(offsetof(WorldContext, player_number) == 0x67C, "WorldContext::player_number offset mismatch");
static_assert(offsetof(WorldContext, player_controlled_character) == 0x680, "WorldContext::player_controlled_character offset mismatch");

}  // namespace gw::context
