#pragma once

#include "base/error_handling.h"

#include "GW/common/constants/constants.h"
#include "GW/common/constants/maps.h"
#include "GW/common/gw_array.h"

#include <cstddef>
#include <cstdint>

namespace GW::Context {

    struct ObserverMatch;

    struct CharProgressBar {
        int     pips;
        uint8_t color[4];      // RGBA
        uint8_t background[4]; // RGBA
        int     unk[7];
        float   progress;      // 0 ... 1
        // possibly more
    };
    static_assert(sizeof(CharProgressBar) == 0x2C, "CharProgressBar size mismatch");

    struct CharContext { // total: 0x458
        /* +h0000 */ GW::GWArray<void*> h0000;
        /* +h0010 */ uint32_t h0010;
        /* +h0014 */ GW::GWArray<void*> h0014;
        /* +h0024 */ uint32_t h0024[4];
        /* +h0034 */ GW::GWArray<void*> h0034;
        /* +h0044 */ GW::GWArray<void*> h0044;
        /* +h0054 */ uint32_t h0054[4]; // load head variables
        /* +h0064 */ uint32_t player_uuid[4]; // uuid
        /* +h0074 */ wchar_t player_name[0x14];
        /* +h009C */ uint32_t h009C[20];
        /* +h00EC */ GW::GWArray<void*> h00EC;
        /* +h00FC */ uint32_t h00FC[37]; // 40
        /* +h0190 */ uint32_t world_flags;
        /* +h0194 */ uint32_t token1; // world id
        /* +h0198 */ GW::Constants::MapID map_id;
        /* +h019C */ uint32_t is_explorable;
        /* +h01A0 */ uint8_t host[0x18];
        /* +h01B8 */ uint32_t token2; // player id
        /* +h01BC */ uint32_t h01BC[27];
        /* +h0228 */ uint32_t h0228;
        /* +h022C */ int32_t district_number;
        /* +h0230 */ GW::Constants::Language language;
        /* +h0234 */ GW::Constants::MapID observe_map_id;
        /* +h0238 */ GW::Constants::MapID current_map_id;
        /* +h023C */ GW::Constants::InstanceType observe_map_type;
        /* +h0240 */ GW::Constants::InstanceType current_map_type;
        /* +h0244 */ uint32_t h0244[5];
        /* +h0258 */ GW::GWArray<ObserverMatch*> observer_matches;
        /* +h0268 */ uint32_t h0268[17];
        /* +h02AC */ uint32_t player_flags; // bitwise something
        /* +h02B0 */ uint32_t player_number;
        /* +h02B4 */ uint32_t h02B4[43];
        /* +h0360 */ CharProgressBar* progress_bar; // seems to never be nullptr
        /* +h0364 */ uint32_t h0364[29];
        /* +h03D8 */ wchar_t player_email[0x40];
    };
    static_assert(offsetof(CharContext, district_number) == 0x22C, "CharContext district offset mismatch");
    static_assert(offsetof(CharContext, progress_bar) == 0x360, "CharContext progress bar offset mismatch");
    static_assert(offsetof(CharContext, player_email) == 0x3D8, "CharContext email offset mismatch");
    static_assert(sizeof(CharContext) == 0x458, "struct CharContext has incorrect size");
}
