#include "base/error_handling.h"

#include "GW/agent_recolor/agent_recolor.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/patterns.h"

namespace GW::agent_recolor {

bool ResolveFindCharFunction() {
    CrashContextScope context("startup", "agent_recolor", "resolve_find_char_func");
    PY4GW::Patterns::Resolve("agent_recolor.find_char_func", &g_find_char);
    return Logger::AssertAddress(
        "ManagerFindChar_Func",
        reinterpret_cast<uintptr_t>(g_find_char),
        "agent_recolor");
}

bool ResolveConsiderColorResolver() {
    CrashContextScope context("startup", "agent_recolor", "resolve_consider_color_resolver");
    PY4GW::Patterns::Resolve("agent_recolor.consider_color_resolver_func", &g_resolver);
    return Logger::AssertAddress(
        "GetConsiderColor_Resolver_Func",
        reinterpret_cast<uintptr_t>(g_resolver),
        "agent_recolor");
}

bool ResolveCharGetTextData() {
    CrashContextScope context("startup", "agent_recolor", "resolve_char_get_text_data");
    PY4GW::Patterns::Resolve("agent_recolor.char_get_text_data_func", &g_char_get_text_data);
    return Logger::AssertAddress(
        "CCharAgent_GetTextData_Func",
        reinterpret_cast<uintptr_t>(g_char_get_text_data),
        "agent_recolor");
}

bool ResolveGadgetGetTextData() {
    CrashContextScope context("startup", "agent_recolor", "resolve_gadget_get_text_data");
    PY4GW::Patterns::Resolve("agent_recolor.gadget_get_text_data_func", &g_gadget_get_text_data);
    return Logger::AssertAddress(
        "CGadgetAgent_GetTextData_Func",
        reinterpret_cast<uintptr_t>(g_gadget_get_text_data),
        "agent_recolor");
}

bool ResolveItemGetTextData() {
    CrashContextScope context("startup", "agent_recolor", "resolve_item_get_text_data");
    PY4GW::Patterns::Resolve("agent_recolor.item_get_text_data_func", &g_item_get_text_data);
    return Logger::AssertAddress(
        "CItemAgent_GetTextData_Func",
        reinterpret_cast<uintptr_t>(g_item_get_text_data),
        "agent_recolor");
}

bool ResolveSetNameTagVisibility() {
    CrashContextScope context("startup", "agent_recolor", "resolve_set_nametag_visibility");
    PY4GW::Patterns::Resolve("agent_recolor.set_global_nametag_visibility_func", &g_set_nametag_visibility);
    // Non-fatal: if this misses, recolor still works (tags refresh on hover/state change).
    return g_set_nametag_visibility != nullptr;
}

bool ResolveNameTagFlags() {
    CrashContextScope context("startup", "agent_recolor", "resolve_nametag_flags");
    PY4GW::Patterns::Resolve("agent_recolor.global_nametag_visibility_flags", &g_nametag_flags);
    return g_nametag_flags != nullptr;
}

}  // namespace GW::agent_recolor
