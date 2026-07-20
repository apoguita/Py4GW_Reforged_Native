#include "base/py_bindings.h"

#include "GW/name_obfuscator/name_obfuscator.h"

namespace py = pybind11;

using GW::name_obfuscator::NameObfuscator;

PYBIND11_EMBEDDED_MODULE(PyNameObfuscator, m) {
    m.doc() = "Player name obfuscator runtime control. DLL initialization owns hooks; Python controls enable/disable, aliases, surfaces, and caches.";

    py::class_<NameObfuscator::ObservedPlayer>(m, "ObservedPlayer")
        .def_readonly("player_number", &NameObfuscator::ObservedPlayer::player_number)
        .def_readonly("agent_id", &NameObfuscator::ObservedPlayer::agent_id)
        .def_readonly("real_name", &NameObfuscator::ObservedPlayer::real_name)
        .def_readonly("display_name", &NameObfuscator::ObservedPlayer::display_name)
        .def_readonly("aliased", &NameObfuscator::ObservedPlayer::aliased);

    m.def("enable", []() { NameObfuscator::Instance().Enable(); },
        "Turn name obfuscation on. Hooks are owned by DLL initialization.");
    m.def("disable", []() { NameObfuscator::Instance().Disable(); },
        "Turn name obfuscation off. Names already cached by the game persist until re-zone.");
    m.def("is_enabled", []() { return NameObfuscator::Instance().IsEnabled(); });
    m.def("is_map_ready", []() { return NameObfuscator::Instance().IsMapReady(); });

    m.def("set_alias",
        [](const std::wstring& real, const std::wstring& fake) {
            NameObfuscator::Instance().SetAlias(real, fake);
        },
        py::arg("real_name"), py::arg("fake_name"),
        "Register or update a real -> fake player-name mapping.");

    m.def("remove_alias",
        [](const std::wstring& real) { return NameObfuscator::Instance().RemoveAlias(real); },
        py::arg("real_name"));

    m.def("clear_aliases", []() { NameObfuscator::Instance().ClearAliases(); },
        "Drop every alias.");
    m.def("clear", []() { NameObfuscator::Instance().ClearAliases(); },
        "Alias for clear_aliases().");

    m.def("alias_count", []() { return (uint32_t)NameObfuscator::Instance().AliasCount(); });
    m.def("get_aliases", []() { return NameObfuscator::Instance().GetAliases(); });

    m.def("get_real_name",
        [](const std::wstring& display) { return NameObfuscator::Instance().ResolveRealName(display); },
        py::arg("display_name"),
        "Resolve an obfuscated display name back to the real name (observed cache, then alias reverse).");
    m.def("get_display_name",
        [](const std::wstring& real) { return NameObfuscator::Instance().ResolveDisplayName(real); },
        py::arg("real_name"),
        "Resolve a real name to its current display (observed cache, then alias).");
    m.def("require_real_name",
        [](const std::wstring& name) { return NameObfuscator::Instance().ResolveRealName(name); },
        py::arg("name"),
        "Return the resolved real name if known, otherwise the input unchanged.");

    m.def("set_surface_enabled",
        [](const std::string& surface, bool on) { return NameObfuscator::Instance().SetSurfaceEnabled(surface, on); },
        py::arg("surface"), py::arg("enabled"),
        "Toggle a single name surface. Returns False for an unknown surface name.");
    m.def("is_surface_enabled",
        [](const std::string& surface) { return NameObfuscator::Instance().IsSurfaceEnabled(surface); },
        py::arg("surface"));
    m.def("list_surfaces", []() { return NameObfuscator::Instance().ListSurfaces(); });

    m.def("scrub_guild_roster", []() { return NameObfuscator::Instance().ScrubGuildRoster(); },
        "Phase 4 fallback (not yet implemented): scrub the durable guild member table; returns count scrubbed.");
    m.def("scrub_guild_identity", []() { return NameObfuscator::Instance().ScrubGuildIdentity(); },
        "Rewrite already-loaded guild name+tag (Guild struct) for aliased guilds; returns guilds changed. "
        "Use after aliasing a guild so masking applies without re-zoning.");

    m.def("clear_observed_cache", []() { NameObfuscator::Instance().ClearObservedCache(); },
        "Drop the map-scoped observed player cache.");
    m.def("observed_count", []() { return (uint32_t)NameObfuscator::Instance().ObservedCount(); });
    m.def("get_observed_players", []() { return NameObfuscator::Instance().GetObservedPlayers(); });
    m.def("get_diagnostics", []() {
        const auto diag = NameObfuscator::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = diag.initialized;
        out["player_join_hook_registered"] = diag.player_join_hook_registered;
        out["class_observer_hook_registered"] = diag.class_observer_hook_registered;
        out["enabled"] = diag.enabled;
        out["current_map_ready"] = diag.current_map_ready;
        out["player_packets_seen"] = diag.player_packets_seen;
        out["player_packets_empty_name"] = diag.player_packets_empty_name;
        out["player_packets_disabled"] = diag.player_packets_disabled;
        out["player_packets_map_not_ready"] = diag.player_packets_map_not_ready;
        out["observed_captures"] = diag.observed_captures;
        out["observed_trylock_skips"] = diag.observed_trylock_skips;
        out["alias_hits"] = diag.alias_hits;
        out["class_observer_hits"] = diag.class_observer_hits;
        out["message_global_hits"] = diag.message_global_hits;
        out["item_custom_hits"] = diag.item_custom_hits;
        out["mercenary_hits"] = diag.mercenary_hits;
        out["mercenary_self_skips"] = diag.mercenary_self_skips;
        out["guild_info_hits"] = diag.guild_info_hits;
        out["party_search_hits"] = diag.party_search_hits;
        out["acct_name_hits"] = diag.acct_name_hits;
        out["acct_name_self_skips"] = diag.acct_name_self_skips;
        out["score_summary_hits"] = diag.score_summary_hits;
        out["score_summary_mode_skips"] = diag.score_summary_mode_skips;
        out["score_summary_self_skips"] = diag.score_summary_self_skips;
        out["guild_charname_hits"] = diag.guild_charname_hits;
        out["guild_identity_hits"] = diag.guild_identity_hits;
        out["guild_invite_hits"] = diag.guild_invite_hits;
        out["guild_motd_hits"] = diag.guild_motd_hits;
        out["own_name_hits"] = diag.own_name_hits;
        out["reverse_alias_collisions"] = diag.reverse_alias_collisions;
        return out;
    });
    m.def("reset_diagnostics", []() { NameObfuscator::Instance().ResetDiagnostics(); });
}
