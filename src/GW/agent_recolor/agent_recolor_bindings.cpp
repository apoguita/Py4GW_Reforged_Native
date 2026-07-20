#include "base/py_bindings.h"

#include "GW/agent_recolor/agent_recolor.h"

namespace py = pybind11;

using GW::agent_recolor::AgentRecolor;

PYBIND11_EMBEDDED_MODULE(PyAgentRecolor, m) {
    m.doc() =
        "Overhead name-tag color override for living agents, gadgets, and ground items - one module.\n"
        "DLL init owns three detours (the living-agent color resolver, CGadgetAgent::GetTextData, and\n"
        "CItemAgent::GetTextData); Python controls per-category enable/disable and the rule stores.\n"
        "Colors are ARGB 0xAARRGGBB (opaque red = 0xFFFF0000). Allegiance ids 1=Ally..6=NpcMinipet.\n"
        "Item rarity ints 0=White 1=Blue 2=Purple 3=Gold 4=Green; item_type is GW::Constants::ItemType.\n"
        "Item match precedence: agent_id > item_id > model_id > name > type > rarity.";

    // ===================== Living agents =====================
    m.def("enable", []() { AgentRecolor::Instance().Enable(); },
        "Enable living-agent color overriding.");
    m.def("disable", []() { AgentRecolor::Instance().Disable(); },
        "Disable living-agent color overriding (game default colors return).");
    m.def("is_enabled", []() { return AgentRecolor::Instance().IsEnabled(); });
    m.def("is_hook_installed", []() { return AgentRecolor::Instance().IsHookInstalled(); },
        "True if the living-agent resolver detour installed at DLL init.");

    m.def("set_agent_color",
        [](uint32_t agent_id, uint32_t argb) { AgentRecolor::Instance().SetAgentColor(agent_id, argb); },
        py::arg("agent_id"), py::arg("argb"),
        "Override one living agent's name-tag color (ARGB). Highest precedence.");
    m.def("remove_agent_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().RemoveAgentColor(agent_id); }, py::arg("agent_id"));
    m.def("set_allegiance_color",
        [](int allegiance, uint32_t argb) { AgentRecolor::Instance().SetAllegianceColor(allegiance, argb); },
        py::arg("allegiance"), py::arg("argb"),
        "Override a whole allegiance category (1=Ally..6=NpcMinipet). Per-agent rules win.");
    m.def("remove_allegiance_color",
        [](int allegiance) { return AgentRecolor::Instance().RemoveAllegianceColor(allegiance); }, py::arg("allegiance"));
    m.def("clear_rules", []() { AgentRecolor::Instance().ClearRules(); },
        "Drop living-agent color overrides.");
    m.def("get_agent_rules", []() { return AgentRecolor::Instance().GetAgentRules(); });
    m.def("get_allegiance_rules", []() { return AgentRecolor::Instance().GetAllegianceRules(); });
    m.def("read_consider_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().ReadConsiderColor(agent_id); }, py::arg("agent_id"),
        "The color the game currently computes for an agent (ARGB), via the original resolver.");

    // ===================== Gadgets =====================
    m.def("gadget_enable", []() { AgentRecolor::Instance().GadgetEnable(); },
        "Enable gadget color overriding.");
    m.def("gadget_disable", []() { AgentRecolor::Instance().GadgetDisable(); },
        "Disable gadget color overriding (game default yellow returns).");
    m.def("gadget_is_enabled", []() { return AgentRecolor::Instance().GadgetIsEnabled(); });
    m.def("gadget_is_hook_installed", []() { return AgentRecolor::Instance().GadgetIsHookInstalled(); });
    m.def("set_gadget_color",
        [](uint32_t agent_id, uint32_t argb) { AgentRecolor::Instance().SetGadgetColor(agent_id, argb); },
        py::arg("agent_id"), py::arg("argb"), "Override one gadget's tag color by agent id. Highest precedence.");
    m.def("remove_gadget_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().RemoveGadgetColor(agent_id); }, py::arg("agent_id"));
    m.def("set_all_gadget_color",
        [](uint32_t argb) { AgentRecolor::Instance().SetAllGadgetColor(argb); }, py::arg("argb"),
        "Recolor every gadget name tag. Per-gadget rules still win.");
    m.def("clear_all_gadget_color", []() { AgentRecolor::Instance().ClearAllGadgetColor(); });
    m.def("gadget_clear_rules", []() { AgentRecolor::Instance().GadgetClearRules(); });
    m.def("get_gadget_rules", []() { return AgentRecolor::Instance().GetGadgetRules(); });
    m.def("has_all_gadget_color", []() { return AgentRecolor::Instance().HasAllGadgetColor(); });
    m.def("get_all_gadget_color", []() { return AgentRecolor::Instance().GetAllGadgetColor(); });

    // ===================== Ground items =====================
    m.def("item_enable", []() { AgentRecolor::Instance().ItemEnable(); },
        "Enable ground-item color overriding.");
    m.def("item_disable", []() { AgentRecolor::Instance().ItemDisable(); },
        "Disable ground-item color overriding (game default label colors return).");
    m.def("item_is_enabled", []() { return AgentRecolor::Instance().ItemIsEnabled(); });
    m.def("item_is_hook_installed", []() { return AgentRecolor::Instance().ItemIsHookInstalled(); });

    m.def("set_item_agent_color",
        [](uint32_t agent_id, uint32_t argb) { AgentRecolor::Instance().SetItemAgentColor(agent_id, argb); },
        py::arg("agent_id"), py::arg("argb"), "Recolor one ground-item instance by agent id (highest precedence).");
    m.def("remove_item_agent_color",
        [](uint32_t agent_id) { return AgentRecolor::Instance().RemoveItemAgentColor(agent_id); }, py::arg("agent_id"));
    m.def("set_item_id_color",
        [](uint32_t item_id, uint32_t argb) { AgentRecolor::Instance().SetItemIdColor(item_id, argb); },
        py::arg("item_id"), py::arg("argb"), "Recolor by item id.");
    m.def("remove_item_id_color",
        [](uint32_t item_id) { return AgentRecolor::Instance().RemoveItemIdColor(item_id); }, py::arg("item_id"));
    m.def("set_item_model_color",
        [](uint32_t model_id, uint32_t argb) { AgentRecolor::Instance().SetItemModelColor(model_id, argb); },
        py::arg("model_id"), py::arg("argb"), "Recolor every item of a model id (item 'kind').");
    m.def("remove_item_model_color",
        [](uint32_t model_id) { return AgentRecolor::Instance().RemoveItemModelColor(model_id); }, py::arg("model_id"));
    m.def("set_item_name_color",
        [](const std::wstring& substring, uint32_t argb) { AgentRecolor::Instance().SetItemNameColor(substring, argb); },
        py::arg("substring"), py::arg("argb"),
        "Recolor items whose decoded name contains `substring` (case-insensitive). Names resolve "
        "asynchronously, so a rule may take a frame or two to take effect per item kind.");
    m.def("remove_item_name_color",
        [](const std::wstring& substring) { return AgentRecolor::Instance().RemoveItemNameColor(substring); },
        py::arg("substring"));
    m.def("set_item_type_color",
        [](int item_type, uint32_t argb) { AgentRecolor::Instance().SetItemTypeColor(item_type, argb); },
        py::arg("item_type"), py::arg("argb"), "Recolor by GW::Constants::ItemType (int).");
    m.def("remove_item_type_color",
        [](int item_type) { return AgentRecolor::Instance().RemoveItemTypeColor(item_type); }, py::arg("item_type"));
    m.def("set_item_rarity_color",
        [](int rarity, uint32_t argb) { AgentRecolor::Instance().SetItemRarityColor(rarity, argb); },
        py::arg("rarity"), py::arg("argb"), "Recolor by rarity (0=White 1=Blue 2=Purple 3=Gold 4=Green).");
    m.def("remove_item_rarity_color",
        [](int rarity) { return AgentRecolor::Instance().RemoveItemRarityColor(rarity); }, py::arg("rarity"));
    m.def("item_clear_rules", []() { AgentRecolor::Instance().ItemClearRules(); }, "Drop all item color rules.");

    m.def("get_item_agent_rules", []() { return AgentRecolor::Instance().GetItemAgentRules(); });
    m.def("get_item_id_rules", []() { return AgentRecolor::Instance().GetItemIdRules(); });
    m.def("get_item_model_rules", []() { return AgentRecolor::Instance().GetItemModelRules(); });
    m.def("get_item_type_rules", []() { return AgentRecolor::Instance().GetItemTypeRules(); });
    m.def("get_item_rarity_rules", []() { return AgentRecolor::Instance().GetItemRarityRules(); });
    m.def("get_item_name_rules", []() {
        py::list out;
        for (const auto& rule : AgentRecolor::Instance().GetItemNameRules())
            out.append(py::make_tuple(rule.first, rule.second));
        return out;
    });

    // ===================== Shared =====================
    m.def("clear_all_rules", []() { AgentRecolor::Instance().ClearAllRules(); },
        "Drop every color rule across agents, gadgets, and items.");

    m.def("get_diagnostics", []() {
        const auto d = AgentRecolor::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = d.initialized;
        out["agent_hook_installed"] = d.agent_hook_installed;
        out["char_tag_hook_installed"] = d.char_tag_hook_installed;
        out["gadget_hook_installed"] = d.gadget_hook_installed;
        out["item_hook_installed"] = d.item_hook_installed;
        out["agent_enabled"] = d.agent_enabled;
        out["gadget_enabled"] = d.gadget_enabled;
        out["item_enabled"] = d.item_enabled;
        out["resolver_calls_seen"] = d.resolver_calls_seen;
        out["agent_rule_hits"] = d.agent_rule_hits;
        out["allegiance_rule_hits"] = d.allegiance_rule_hits;
        out["last_agent_id"] = d.last_agent_id;
        out["last_agent_color"] = d.last_agent_color;
        out["gadget_calls_seen"] = d.gadget_calls_seen;
        out["gadget_rule_hits"] = d.gadget_rule_hits;
        out["gadget_all_hits"] = d.gadget_all_hits;
        out["last_gadget_id"] = d.last_gadget_id;
        out["last_gadget_color"] = d.last_gadget_color;
        out["item_calls_seen"] = d.item_calls_seen;
        out["item_rule_hits"] = d.item_rule_hits;
        out["last_item_id"] = d.last_item_id;
        out["last_item_model"] = d.last_item_model;
        out["last_item_color"] = d.last_item_color;
        out["item_name_cache_size"] = d.item_name_cache_size;
        return out;
    });
    m.def("reset_diagnostics", []() { AgentRecolor::Instance().ResetDiagnostics(); });

    // Backward-compat: legacy agent-only helper names retained.
    m.def("get_agent_diagnostics", []() {
        const auto d = AgentRecolor::Instance().GetDiagnostics();
        py::dict out;
        out["initialized"] = d.initialized;
        out["hook_installed"] = d.agent_hook_installed;
        out["enabled"] = d.agent_enabled;
        out["resolver_calls_seen"] = d.resolver_calls_seen;
        out["agent_rule_hits"] = d.agent_rule_hits;
        out["allegiance_rule_hits"] = d.allegiance_rule_hits;
        out["last_agent_id"] = d.last_agent_id;
        out["last_color"] = d.last_agent_color;
        return out;
    });
}
