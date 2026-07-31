#include "base/error_handling.h"

#include "GW/ui/ui.h"

#include "base/CrashHandler.h"
#include "base/hooker.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"

#include <shellapi.h>

namespace GW::Context {
extern uintptr_t g_world_map_state_addr;
extern uintptr_t g_preferences_initialized_addr;
extern uintptr_t g_title_table_addr;
extern uintptr_t g_ui_drawn_addr;
extern uintptr_t g_shift_screen_addr;
extern uintptr_t g_game_settings_addr;
extern EnumPreferenceInfo* g_enum_preference_options_addr;
extern NumberPreferenceInfo* g_number_preference_options_addr;
extern GW::GWArray<ui::Frame*>* g_frame_array;
extern ui::TooltipInfo*** g_current_tooltip_ptr;
extern WindowPosition* g_window_positions_array;
}

namespace GW::ui {

using SendUIMessageFn = void(__cdecl*)(UIMessage message_id, void* wparam, void* lparam);
using SendFrameUIMessageFn = void(__fastcall*)(GW::GWArray<UIInteractionCallback>* callbacks, void* edx, UIMessage message_id, void* wparam, void* lparam);
using SendFrameUIMessageByIdFn = void(__cdecl*)(uint32_t frame_id, UIMessage message_id, void* wparam, void* lparam);
using CreateHashFromWcharFn = uint32_t(__cdecl*)(const wchar_t* value, int seed);
using GetChildFrameIdFn = uint32_t(__cdecl*)(uint32_t parent_frame_id, uint32_t child_offset);
using FindRelatedFrameFn = uint32_t(__cdecl*)(uint32_t frame_id, uint32_t relation_kind, uint32_t start_after_id);
using GetRootFrameFn = Frame*(__cdecl*)();
using LoadSettingsFn = void(__cdecl*)(uint32_t size, uint8_t* data);
using SetWindowVisibleFn = void(__cdecl*)(uint32_t window_id, uint32_t is_visible, void* wparam, void* lparam);
using SetWindowPositionFn = void(__cdecl*)(uint32_t window_id, Context::WindowPosition* info, void* wparam, void* lparam);
using ValidateAsyncDecodeStrFn = void(__cdecl*)(const wchar_t* value, DecodeStr_Callback callback, void* wparam);
using DoAsyncDecodeStrFn = uint32_t(__fastcall*)(void* ecx, void* edx, wchar_t* encoded_str, DecodeStr_Callback callback, void* wparam);
using TitleBinarySearchFn = uint32_t(__fastcall*)(void* table, void* edx, void* key, uint32_t* result_entry);
using GetTitleFn = const wchar_t*(__fastcall*)(void* nonclient);
using DrawOnCompassFn = void(__cdecl*)(uint32_t session_id, uint32_t point_count, CompassPoint* points);
using CreateUIComponentFn = uint32_t(__cdecl*)(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label);
using DestroyUIComponentFn = bool(__cdecl*)(uint32_t frame_id);
using FrameNewSubclassFn = uint32_t(__cdecl*)(uint32_t frame_id, void* subclass_proc, uint32_t msg_id);
using SetTooltipFn = void(__cdecl*)(TooltipInfo** tooltip);
using TypedComponentPassthroughFn = void(__cdecl*)(void* param_1, void* param_2, void* param_3, void* param_4, void* param_5);
using GetFlagPreferenceFn = bool(__cdecl*)(uint32_t flag_pref_id);
using SetFlagPreferenceFn = void(__cdecl*)(uint32_t flag_pref_id, bool value);
using GetStringPreferenceFn = wchar_t*(__cdecl*)(uint32_t string_pref_id);
using SetStringPreferenceFn = void(__cdecl*)(uint32_t string_pref_id, wchar_t* value);
using GetEnumPreferenceFn = uint32_t(__cdecl*)(uint32_t choice_pref_id);
using SetEnumPreferenceFn = void(__cdecl*)(uint32_t choice_pref_id, uint32_t value);
using GetNumberPreferenceFn = uint32_t(__cdecl*)(uint32_t number_pref_id);
using SetNumberPreferenceFn = void(__cdecl*)(uint32_t number_pref_id, uint32_t value);
using GetGraphicsRendererValueFn = uint32_t(__cdecl*)(void* graphics_renderer_ptr, uint32_t metric_id);
using SetGraphicsRendererValueFn = void(__cdecl*)(void* graphics_renderer, uint32_t renderer_mode, uint32_t metric_id, uint32_t value);
using GetGameRendererModeFn = uint32_t(__cdecl*)(uint32_t game_renderer_context);
using SetGameRendererModeFn = void(__cdecl*)(uint32_t game_renderer_context, uint32_t game_renderer_mode);
using GetGameRendererMetricFn = uint32_t(__cdecl*)(uint32_t game_renderer_context, uint32_t game_renderer_mode, uint32_t metric_key);
using SetInGameShadowQualityFn = void(__cdecl*)(uint32_t value);
using SetInGameStaticPreferenceFn = void(__cdecl*)(uint32_t static_preference_id, uint32_t value);
using TriggerTerrainRerenderFn = void(__cdecl*)();
using SetInGameUIScaleFn = void(__cdecl*)(uint32_t value);
using SetVolumeFn = void(__cdecl*)(uint32_t volume_id, float amount);
using SetMasterVolumeFn = void(__cdecl*)(float amount);

struct UIMessageCallbackEntry {
    int altitude;
    PY4GW::HookEntry* entry;
    UIMessageCallback callback;
};

struct FrameUIMessageCallbackEntry {
    int altitude;
    PY4GW::HookEntry* entry;
    FrameUIMessageCallback callback;
};

struct CreateUIComponentCallbackEntry {
    int altitude;
    PY4GW::HookEntry* entry;
    CreateUIComponentCallback callback;
};

bool ResolveFrameArray();
bool ResolveWorldMapState();
bool ResolveSendFrameUiMessage();
bool ResolveCreateHashFromWchar();
bool ResolveGetChildFrameId();
bool ResolveFindRelatedFrame();
bool ResolveGetRootFrame();
bool ResolveSendUiMessage();
bool ResolveLoadSettings();
bool ResolveUiDrawn();
bool ResolveShiftScreenshot();
bool ResolveSetTooltip();
bool ResolveGameSettings();
bool ResolveWindowHelpers();
bool ResolveValidateAsyncDecode();
bool ResolveTitleHelpers();
bool ResolveDrawOnCompass();
bool ResolveCreateUiComponent();
bool ResolveItemImageFrameTint();
bool ResolveFrameNewSubclass();
bool ResolveTypedComponentPassthrough();
bool ResolvePreferenceReaders();
bool ResolvePreferenceWriters();
bool ResolveCommandLineFunctions();
bool TryResolveTypedComponentCallbacks();

extern SendUIMessageFn g_send_ui_message_func;
extern SendUIMessageFn g_send_ui_message_original;
extern SendFrameUIMessageFn g_send_frame_ui_message_func;
extern SendFrameUIMessageFn g_send_frame_ui_message_original;
extern SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_func;
extern SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_original;
extern CreateHashFromWcharFn g_create_hash_from_wchar_func;
extern GetChildFrameIdFn g_get_child_frame_id_func;
extern FindRelatedFrameFn g_find_related_frame_func;
extern GetRootFrameFn g_get_root_frame_func;
extern LoadSettingsFn g_load_settings_func;
extern SetWindowVisibleFn g_set_window_visible_func;
extern SetWindowPositionFn g_set_window_position_func;
extern ValidateAsyncDecodeStrFn g_validate_async_decode_str_func;
extern DoAsyncDecodeStrFn g_async_decode_string_func;
extern TitleBinarySearchFn g_title_binary_search_func;
extern GetTitleFn g_get_title_func;
extern DrawOnCompassFn g_draw_on_compass_func;
extern CreateUIComponentFn g_create_ui_component_func;
extern CreateUIComponentFn g_create_ui_component_original;
extern DestroyUIComponentFn g_destroy_ui_component_func;
extern FrameNewSubclassFn g_frame_new_subclass_func;
extern TypedComponentPassthroughFn g_typed_component_passthrough_func;
extern GetFlagPreferenceFn g_get_flag_preference_func;
extern SetFlagPreferenceFn g_set_flag_preference_func;
extern GetStringPreferenceFn g_get_string_preference_func;
extern SetStringPreferenceFn g_set_string_preference_func;
extern GetEnumPreferenceFn g_get_enum_preference_func;
extern SetEnumPreferenceFn g_set_enum_preference_func;
extern GetNumberPreferenceFn g_get_number_preference_func;
extern SetNumberPreferenceFn g_set_number_preference_func;
extern GetGraphicsRendererValueFn g_get_graphics_renderer_value_func;
extern SetGraphicsRendererValueFn g_set_graphics_renderer_value_func;
extern GetGameRendererModeFn g_get_game_renderer_mode_func;
extern SetGameRendererModeFn g_set_game_renderer_mode_func;
extern GetGameRendererMetricFn g_get_game_renderer_metric_func;
extern SetInGameShadowQualityFn g_set_in_game_shadow_quality_func;
extern SetInGameStaticPreferenceFn g_set_in_game_static_preference_func;
extern TriggerTerrainRerenderFn g_trigger_terrain_rerender_func;
extern SetInGameUIScaleFn g_set_in_game_ui_scale_func;
extern SetVolumeFn g_set_volume_func;
extern SetMasterVolumeFn g_set_master_volume_func;
extern ItemImageFramePaintFn g_item_image_frame_paint_func;
extern ItemImageFramePaintFn g_item_image_frame_paint_original;
extern ItemImageFrameCtlMsgProcFn g_item_image_frame_ctl_msg_proc_func;
extern ItemImageFrameCtlMsgProcFn g_item_image_frame_ctl_msg_proc_original;
extern GrModelSetColorFn g_gr_model_set_color_func;
extern GrModelSetMaterialConstantFn g_gr_model_set_material_constant_func;
extern GrMaterialConstantGetIdFn g_gr_material_constant_get_id_func;
extern std::unordered_map<uint32_t, uint32_t> g_item_image_frame_tints;
extern std::unordered_map<uint32_t, uint32_t> g_item_image_item_tints;
extern std::unordered_map<uint32_t, uint32_t> g_item_image_frame_by_item_id;
extern std::atomic<bool> g_item_image_frame_tint_hook_installed;
extern std::atomic<bool> g_item_image_frame_tint_enabled;
extern std::atomic<bool> g_item_image_frame_pop_enabled;
extern std::atomic<float> g_item_image_frame_pop_brightness;
extern std::atomic<uint64_t> g_item_image_frame_paint_calls;
extern std::atomic<uint64_t> g_item_image_frame_tint_matches;
extern std::atomic<uint64_t> g_item_image_frame_model_hits;
extern std::atomic<uint64_t> g_item_image_frame_color_calls;
extern std::atomic<uint64_t> g_item_image_frame_icon_constant_calls;
extern std::atomic<uint32_t> g_item_image_frame_material_constant_id;
extern std::atomic<uint32_t> g_item_image_frame_last_frame_id;
extern std::atomic<uintptr_t> g_item_image_frame_last_model;
extern std::atomic<uint32_t> g_item_image_frame_last_item_id;
extern std::atomic<uint64_t> g_item_image_frame_item_bindings;
extern std::atomic<uint32_t> g_item_image_frame_last_bound_item_id;
extern std::atomic<uint32_t> g_item_image_frame_last_bound_native_frame_id;
extern uint32_t* g_command_line_number_buffer;
extern GetFlagPreferenceFn g_get_command_line_flag_func;
extern GetStringPreferenceFn g_get_command_line_string_func;
extern GetNumberPreferenceFn g_get_command_line_number_func;
extern uint32_t g_create_flat_button_dialog_subclass_type;
extern UIInteractionCallback g_button_frame_callback;
extern UIInteractionCallback g_ctl_button_proc_callback;
extern UIInteractionCallback g_text_button_frame_callback;
extern UIInteractionCallback g_scrollable_frame_callback;
extern UIInteractionCallback g_text_label_frame_callback;
extern UIInteractionCallback g_frame_list_callback;
extern UIInteractionCallback g_dropdown_frame_callback;
extern UIInteractionCallback g_slider_frame_callback;
extern UIInteractionCallback g_slider_frame_wrapper_callback;
extern UIInteractionCallback g_editable_text_frame_callback;
extern UIInteractionCallback g_progress_bar_callback;
extern UIInteractionCallback g_tabs_frame_callback;
extern bool g_typed_component_callbacks_initialized;
extern CRITICAL_SECTION g_callback_mutex;
extern bool g_callback_mutex_initialized;
extern std::unordered_map<UIMessage, std::vector<UIMessageCallbackEntry>> g_ui_message_callbacks;
extern std::unordered_map<UIMessage, std::vector<FrameUIMessageCallbackEntry>> g_frame_ui_message_callbacks;
extern std::vector<CreateUIComponentCallbackEntry> g_create_ui_component_callbacks;
extern bool g_open_links;
extern PY4GW::HookEntry g_open_template_hook;
extern std::atomic<bool> g_initialized;
extern std::atomic<bool> g_shutting_down;
extern std::atomic<uint32_t> g_active_hooks;

bool WaitForUiHooksToDrain() {
    CrashContextScope context("shutdown", "ui", "wait_for_hooks_to_drain");
    for (int i = 0; i < 125; ++i) {
        if (g_active_hooks.load() == 0) {
            return true;
        }
        ::Sleep(16);
    }

    Logger::Instance().LogWarning("[ui] Timed out waiting for in-flight UI hooks to drain.", "ui");
    return false;
}

void __cdecl OnSendUIMessage(UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;
    if (!g_shutting_down) {
        SendUIMessage(message_id, wparam, lparam);
    } else if (g_send_ui_message_original) {
        g_send_ui_message_original(message_id, wparam, lparam);
    }
    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

void OnOpenTemplateUiMessage(PY4GW::HookStatus* hook_status, UIMessage msgid, void* wparam, void*) {
    // The game legitimately emits kOpenTemplate with a null wparam during normal
    // frame processing. Guard gracefully instead of fatal-asserting (a null wparam
    // used to crash the whole client here).
    if (msgid != UIMessage::kOpenTemplate || !wparam) {
        return;
    }
    auto* info = static_cast<ChatTemplate*>(wparam);
    if (!(g_open_links && info && info->code.valid() && info->name)) {
        return;
    }
    if (!wcsncmp(info->name, L"http://", 7) || !wcsncmp(info->name, L"https://", 8)) {
        hook_status->blocked = true;
        ::ShellExecuteW(nullptr, L"open", info->name, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void __cdecl OnSendFrameUIMessageById(uint32_t frame_id, UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;
    if (!g_shutting_down) {
        Frame* frame = GetFrameById(frame_id);
        if (frame) {
            SendFrameUIMessage(frame, message_id, wparam, lparam);
        }
    } else if (g_send_frame_ui_message_by_id_original) {
        g_send_frame_ui_message_by_id_original(frame_id, message_id, wparam, lparam);
    }
    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

void __fastcall OnSendFrameUIMessage(GW::GWArray<UIInteractionCallback>* frame_callbacks, void*, UIMessage message_id, void* wparam, void* lparam) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;
    // The callback array can outlive its Frame while inventory controls are
    // being torn down.  Reconstructing Frame at callbacks-0xA8 and routing
    // through SendFrameUIMessage then dereferences that stale object.  For
    // game-originated messages, preserve the original call ABI exactly; the
    // public SendFrameUIMessage API remains available for explicit callers.
    if (g_send_frame_ui_message_original) {
        g_send_frame_ui_message_original(frame_callbacks, nullptr, message_id, wparam, lparam);
    }
    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

uint32_t __cdecl OnCreateUIComponent(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;

    uint32_t result = 0;
    if (g_shutting_down || !g_create_ui_component_original) {
        if (g_create_ui_component_original) {
            result = g_create_ui_component_original(frame_id, component_flags, tab_index, event_callback, name_enc, component_label);
        }
    } else {
        CreateUIComponentPacket packet{
            frame_id,
            component_flags,
            tab_index,
            reinterpret_cast<UIInteractionCallback>(event_callback),
            name_enc,
            component_label};

        std::vector<CreateUIComponentCallbackEntry> callbacks;
        if (g_callback_mutex_initialized) {
            ::EnterCriticalSection(&g_callback_mutex);
            callbacks = g_create_ui_component_callbacks;
            ::LeaveCriticalSection(&g_callback_mutex);
        }

        PY4GW::HookStatus status;
        size_t i = 0;
        for (; i < callbacks.size(); ++i) {
            if (callbacks[i].altitude > 0) {
                break;
            }
            callbacks[i].callback(&packet);
            ++status.altitude;
        }

        result = g_create_ui_component_original(
            packet.frame_id,
            packet.component_flags,
            packet.tab_index,
            reinterpret_cast<void*>(packet.event_callback),
            packet.name_enc,
            packet.component_label);

        for (; i < callbacks.size(); ++i) {
            callbacks[i].callback(&packet);
            ++status.altitude;
        }
    }

    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
    return result;
}

void __fastcall OnItemImageFramePaint(void* item_image_frame, void* edx, const void* paint_state) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;

    // The original constructs/rebuilds the background model. Reapply the
    // user-owned tint afterwards, while the model handle is current.
    if (g_item_image_frame_paint_original) {
        g_item_image_frame_paint_original(item_image_frame, edx, paint_state);
    }

    ++g_item_image_frame_paint_calls;
    if (!g_shutting_down && item_image_frame) {
        const auto base = reinterpret_cast<uintptr_t>(item_image_frame);
        const uint32_t frame_id = *reinterpret_cast<const uint32_t*>(base + 0x04);
        const uint32_t item_id = *reinterpret_cast<const uint32_t*>(base + 0x54);
        void* background_model = *reinterpret_cast<void* const*>(base + 0x2C);
        // The background/rating model at +0x2C is the stable tint target.
        // The icon-model path is explicitly opt-in for staged testing; do
        // not even read its handle unless the experimental toggle is on.
        void* icon_model = g_item_image_frame_pop_enabled
            ? *reinterpret_cast<void* const*>(base + 0x34)
            : nullptr;
        g_item_image_frame_last_frame_id = frame_id;
        g_item_image_frame_last_item_id = item_id;
        g_item_image_frame_last_model = reinterpret_cast<uintptr_t>(background_model);
        g_item_image_frame_last_icon_model = reinterpret_cast<uintptr_t>(icon_model);
        const auto frame_tint = g_item_image_frame_tints.find(frame_id);
        const auto item_tint = g_item_image_item_tints.find(item_id);
        // Prefer the concrete native frame binding, but allow the public
        // item-id API to work without intercepting CItemImageFrame's message
        // dispatcher.  The dispatcher hook was unsafe across inventory
        // teardown/recreation; paint already exposes the current item id.
        const uint32_t* tint_argb = frame_tint != g_item_image_frame_tints.end()
            ? &frame_tint->second
            : (item_tint != g_item_image_item_tints.end() ? &item_tint->second : nullptr);
        if (tint_argb) {
            ++g_item_image_frame_tint_matches;
        }
        if (background_model) {
            ++g_item_image_frame_model_hits;
        }
        if (icon_model) {
            ++g_item_image_frame_icon_model_hits;
        }
        if (g_item_image_frame_tint_enabled && g_gr_model_set_color_func && tint_argb && background_model) {
            ++g_item_image_frame_color_calls;
            g_gr_model_set_color_func(background_model, tint_argb);
        }
        // Stage 1 experimental mode: apply the same validated ARGB setter to
        // the icon model. Shader/material writes remain disabled until this
        // survives inventory teardown/recreation.
        if (g_item_image_frame_tint_enabled && g_item_image_frame_pop_enabled &&
            g_gr_model_set_color_func && tint_argb && icon_model) {
            ++g_item_image_frame_icon_color_calls;
            g_gr_model_set_color_func(icon_model, tint_argb);
        }
    }

    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

void __cdecl OnItemImageFrameCtlMsgProc(const uint32_t* frame_msg_hdr, const uint32_t* message_data, void* out) {
    PY4GW::HookBase::EnterHook();
    ++g_active_hooks;

    if (g_item_image_frame_ctl_msg_proc_original) {
        g_item_image_frame_ctl_msg_proc_original(frame_msg_hdr, message_data, out);
    }

    // CItemImageFrame message 0x5b is the only point where the game gives us
    // both the unique runtime item id (message_data[1]) and the concrete
    // CItemImageFrame instance. Its +0x04 is precisely the native key later
    // consumed by AddBackground, unlike Python's separately-managed UI id.
    if (!g_shutting_down && frame_msg_hdr && message_data && frame_msg_hdr[1] == 0x5b && frame_msg_hdr[2]) {
        const auto instance_slot = reinterpret_cast<void* const*>(static_cast<uintptr_t>(frame_msg_hdr[2]));
        void* const item_image_frame = instance_slot ? *instance_slot : nullptr;
        const uint32_t item_id = message_data[1];
        if (item_image_frame && item_id) {
            const uint32_t native_frame_id = *reinterpret_cast<const uint32_t*>(reinterpret_cast<uintptr_t>(item_image_frame) + 0x04);
            if (native_frame_id) {
                const auto previous = g_item_image_frame_by_item_id.find(item_id);
                if (previous != g_item_image_frame_by_item_id.end() && previous->second != native_frame_id) {
                    g_item_image_frame_tints.erase(previous->second);
                }
                g_item_image_frame_by_item_id[item_id] = native_frame_id;
                ++g_item_image_frame_item_bindings;
                g_item_image_frame_last_bound_item_id = item_id;
                g_item_image_frame_last_bound_native_frame_id = native_frame_id;
                const auto tint = g_item_image_item_tints.find(item_id);
                if (tint != g_item_image_item_tints.end()) {
                    g_item_image_frame_tints[native_frame_id] = tint->second;
                }
            }
        }
    }

    --g_active_hooks;
    PY4GW::HookBase::LeaveHook();
}

bool Init() {
    CrashContextScope context("startup", "ui", "init");
    ::InitializeCriticalSection(&g_callback_mutex);
    g_callback_mutex_initialized = true;
    g_item_image_frame_tint_enabled = true;

    const auto try_resolve = [](const char* name, bool(*resolver)()) {
        if (!resolver()) {
            Logger::Instance().LogWarning(std::string("Optional resolver failed: ") + name, "ui");
        }
    };

    try_resolve("ResolveFrameArray", &ResolveFrameArray);
    try_resolve("ResolveWorldMapState", &ResolveWorldMapState);
    try_resolve("ResolveSendFrameUiMessage", &ResolveSendFrameUiMessage);
    try_resolve("ResolveCreateHashFromWchar", &ResolveCreateHashFromWchar);
    try_resolve("ResolveGetChildFrameId", &ResolveGetChildFrameId);
    try_resolve("ResolveFindRelatedFrame", &ResolveFindRelatedFrame);
    try_resolve("ResolveGetRootFrame", &ResolveGetRootFrame);
    try_resolve("ResolveSendUiMessage", &ResolveSendUiMessage);
    try_resolve("ResolveLoadSettings", &ResolveLoadSettings);
    try_resolve("ResolveUiDrawn", &ResolveUiDrawn);
    try_resolve("ResolveShiftScreenshot", &ResolveShiftScreenshot);
    try_resolve("ResolveSetTooltip", &ResolveSetTooltip);
    try_resolve("ResolveGameSettings", &ResolveGameSettings);
    try_resolve("ResolveWindowHelpers", &ResolveWindowHelpers);
    try_resolve("ResolveValidateAsyncDecode", &ResolveValidateAsyncDecode);
    try_resolve("ResolveTitleHelpers", &ResolveTitleHelpers);
    try_resolve("ResolveDrawOnCompass", &ResolveDrawOnCompass);
    try_resolve("ResolveCreateUiComponent", &ResolveCreateUiComponent);
    try_resolve("ResolveItemImageFrameTint", &ResolveItemImageFrameTint);
    try_resolve("ResolveFrameNewSubclass", &ResolveFrameNewSubclass);
    try_resolve("ResolveTypedComponentPassthrough", &ResolveTypedComponentPassthrough);
    try_resolve("ResolvePreferenceReaders", &ResolvePreferenceReaders);
    try_resolve("ResolvePreferenceWriters", &ResolvePreferenceWriters);
    try_resolve("ResolveCommandLineFunctions", &ResolveCommandLineFunctions);

    if (g_send_ui_message_func) {
        // UI message dispatch has the same teardown-sensitive handle ABI as
        // frame dispatch. Keep the resolved function for explicit calls, but
        // do not detour the game's native message path.
        g_send_ui_message_original = g_send_ui_message_func;
    } else {
        Logger::Instance().LogWarning("SendUIMessage_Func is unavailable; UI message calls will remain disabled.", "ui");
    }

    // Do not detour frame-message dispatch. Its callback-array argument is
    // also used during frame teardown, and the game can legitimately invoke
    // it after the owning Frame has been destroyed. Keep the resolved entry
    // point available for explicit, validated API calls only.
    if (g_send_frame_ui_message_by_id_func) {
        g_send_frame_ui_message_by_id_original = g_send_frame_ui_message_by_id_func;
    } else {
        Logger::Instance().LogWarning("SendFrameUIMessageById_Func is unavailable; frame-by-id calls will remain disabled.", "ui");
    }

    if (g_send_frame_ui_message_func) {
        g_send_frame_ui_message_original = g_send_frame_ui_message_func;
    } else {
        Logger::Instance().LogWarning("SendFrameUIMessage_Func is unavailable; frame message calls will remain disabled.", "ui");
    }

    if (g_create_ui_component_func) {
        Logger::AssertHook(
            "CreateUIComponent_Func",
            PY4GW::HookBase::CreateHook(
                reinterpret_cast<void**>(&g_create_ui_component_func),
                reinterpret_cast<void*>(&OnCreateUIComponent),
                reinterpret_cast<void**>(&g_create_ui_component_original)),
            "ui");
    } else {
        Logger::Instance().LogWarning("CreateUIComponent_Func is unavailable; UI component creation hooks will remain disabled.", "ui");
    }

    if (g_item_image_frame_paint_func && g_gr_model_set_color_func) {
        const bool paint_hook_created = Logger::AssertHook(
            "CItemImageFrame_Paint_Func",
            PY4GW::HookBase::CreateHook(
                reinterpret_cast<void**>(&g_item_image_frame_paint_func),
                reinterpret_cast<void*>(&OnItemImageFramePaint),
                reinterpret_cast<void**>(&g_item_image_frame_paint_original)),
            "ui");
        if (paint_hook_created) {
            g_item_image_frame_tint_hook_installed = true;
        }
    } else {
        Logger::Instance().LogWarning("CItemImageFrame paint or GrModelSetColor is unavailable; item-frame tinting will remain disabled.", "ui");
    }

    return true;
}

void EnableHooks() {
    CrashContextScope context("runtime", "ui", "enable_hooks");
    if (g_create_ui_component_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_create_ui_component_func));
    }
    if (g_item_image_frame_paint_func) {
        PY4GW::HookBase::EnableHooks(reinterpret_cast<void*>(g_item_image_frame_paint_func));
    }
    RegisterUIMessageCallback(&g_open_template_hook, UIMessage::kOpenTemplate, &OnOpenTemplateUiMessage);
}

void DisableHooks() {
    CrashContextScope context("shutdown", "ui", "disable_hooks");
    RemoveUIMessageCallback(&g_open_template_hook);
    if (g_create_ui_component_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_create_ui_component_func));
    }
    if (g_item_image_frame_paint_func) {
        PY4GW::HookBase::DisableHooks(reinterpret_cast<void*>(g_item_image_frame_paint_func));
    }
}

void Exit() {
    CrashContextScope context("shutdown", "ui", "exit");
    if (g_callback_mutex_initialized) {
        ::EnterCriticalSection(&g_callback_mutex);
        g_ui_message_callbacks.clear();
        g_frame_ui_message_callbacks.clear();
        g_create_ui_component_callbacks.clear();
        ::LeaveCriticalSection(&g_callback_mutex);
    }

    if (g_create_ui_component_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_create_ui_component_func));
    }
    if (g_item_image_frame_paint_func) {
        PY4GW::HookBase::RemoveHook(reinterpret_cast<void*>(g_item_image_frame_paint_func));
    }

    if (g_callback_mutex_initialized) {
        ::DeleteCriticalSection(&g_callback_mutex);
        g_callback_mutex_initialized = false;
    }

    g_send_ui_message_func = nullptr;
    g_send_ui_message_original = nullptr;
    g_send_frame_ui_message_func = nullptr;
    g_send_frame_ui_message_original = nullptr;
    g_send_frame_ui_message_by_id_func = nullptr;
    g_send_frame_ui_message_by_id_original = nullptr;
    g_create_hash_from_wchar_func = nullptr;
    g_get_child_frame_id_func = nullptr;
    g_find_related_frame_func = nullptr;
    g_get_root_frame_func = nullptr;
    g_load_settings_func = nullptr;
    g_set_window_visible_func = nullptr;
    g_set_window_position_func = nullptr;
    g_validate_async_decode_str_func = nullptr;
    g_async_decode_string_func = nullptr;
    g_title_binary_search_func = nullptr;
    g_get_title_func = nullptr;
    g_draw_on_compass_func = nullptr;
    g_create_ui_component_func = nullptr;
    g_create_ui_component_original = nullptr;
    g_item_image_frame_paint_func = nullptr;
    g_item_image_frame_paint_original = nullptr;
    g_item_image_frame_ctl_msg_proc_func = nullptr;
    g_item_image_frame_ctl_msg_proc_original = nullptr;
    g_gr_model_set_color_func = nullptr;
    g_gr_model_set_material_constant_func = nullptr;
    g_gr_material_constant_get_id_func = nullptr;
    g_item_image_frame_tints.clear();
    g_item_image_item_tints.clear();
    g_item_image_frame_by_item_id.clear();
    g_item_image_frame_tint_hook_installed = false;
    g_item_image_frame_tint_enabled = true;
    g_item_image_frame_pop_enabled = false;
    g_item_image_frame_pop_brightness = 1.35f;
    g_item_image_frame_paint_calls = 0;
    g_item_image_frame_tint_matches = 0;
    g_item_image_frame_model_hits = 0;
    g_item_image_frame_color_calls = 0;
    g_item_image_frame_icon_model_hits = 0;
    g_item_image_frame_icon_color_calls = 0;
    g_item_image_frame_icon_constant_calls = 0;
    g_item_image_frame_material_constant_id = UINT32_MAX;
    g_item_image_frame_last_frame_id = 0;
    g_item_image_frame_last_model = 0;
    g_item_image_frame_last_icon_model = 0;
    g_item_image_frame_last_item_id = 0;
    g_item_image_frame_item_bindings = 0;
    g_item_image_frame_last_bound_item_id = 0;
    g_item_image_frame_last_bound_native_frame_id = 0;
    g_destroy_ui_component_func = nullptr;
    g_frame_new_subclass_func = nullptr;
    g_typed_component_passthrough_func = nullptr;
    g_get_flag_preference_func = nullptr;
    g_set_flag_preference_func = nullptr;
    g_get_string_preference_func = nullptr;
    g_set_string_preference_func = nullptr;
    g_get_enum_preference_func = nullptr;
    g_set_enum_preference_func = nullptr;
    g_get_number_preference_func = nullptr;
    g_set_number_preference_func = nullptr;
    g_get_graphics_renderer_value_func = nullptr;
    g_set_graphics_renderer_value_func = nullptr;
    g_get_game_renderer_mode_func = nullptr;
    g_set_game_renderer_mode_func = nullptr;
    g_get_game_renderer_metric_func = nullptr;
    g_set_in_game_shadow_quality_func = nullptr;
    g_set_in_game_static_preference_func = nullptr;
    g_trigger_terrain_rerender_func = nullptr;
    g_set_in_game_ui_scale_func = nullptr;
    g_set_volume_func = nullptr;
    g_set_master_volume_func = nullptr;
    Context::g_enum_preference_options_addr = nullptr;
    Context::g_number_preference_options_addr = nullptr;
    g_command_line_number_buffer = nullptr;
    g_get_command_line_flag_func = nullptr;
    g_get_command_line_string_func = nullptr;
    g_get_command_line_number_func = nullptr;
    g_button_frame_callback = nullptr;
    g_ctl_button_proc_callback = nullptr;
    g_text_button_frame_callback = nullptr;
    g_scrollable_frame_callback = nullptr;
    g_text_label_frame_callback = nullptr;
    g_frame_list_callback = nullptr;
    g_dropdown_frame_callback = nullptr;
    g_slider_frame_callback = nullptr;
    g_slider_frame_wrapper_callback = nullptr;
    g_editable_text_frame_callback = nullptr;
    g_progress_bar_callback = nullptr;
    g_tabs_frame_callback = nullptr;
    g_typed_component_callbacks_initialized = false;
    Context::g_frame_array = nullptr;
    Context::g_world_map_state_addr = 0;
    Context::g_preferences_initialized_addr = 0;
    Context::g_title_table_addr = 0;
    Context::g_ui_drawn_addr = 0;
    Context::g_shift_screen_addr = 0;
    Context::g_game_settings_addr = 0;
    Context::g_current_tooltip_ptr = nullptr;
    Context::g_window_positions_array = nullptr;
    g_open_links = false;
    g_active_hooks = 0;
}

SendUIMessageFn g_send_ui_message_func = nullptr;
SendUIMessageFn g_send_ui_message_original = nullptr;
SendFrameUIMessageFn g_send_frame_ui_message_func = nullptr;
SendFrameUIMessageFn g_send_frame_ui_message_original = nullptr;
SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_func = nullptr;
SendFrameUIMessageByIdFn g_send_frame_ui_message_by_id_original = nullptr;
CreateHashFromWcharFn g_create_hash_from_wchar_func = nullptr;
GetChildFrameIdFn g_get_child_frame_id_func = nullptr;
FindRelatedFrameFn g_find_related_frame_func = nullptr;
GetRootFrameFn g_get_root_frame_func = nullptr;
LoadSettingsFn g_load_settings_func = nullptr;
SetWindowVisibleFn g_set_window_visible_func = nullptr;
SetWindowPositionFn g_set_window_position_func = nullptr;
ValidateAsyncDecodeStrFn g_validate_async_decode_str_func = nullptr;
DoAsyncDecodeStrFn g_async_decode_string_func = nullptr;
TitleBinarySearchFn g_title_binary_search_func = nullptr;
GetTitleFn g_get_title_func = nullptr;
DrawOnCompassFn g_draw_on_compass_func = nullptr;
CreateUIComponentFn g_create_ui_component_func = nullptr;
CreateUIComponentFn g_create_ui_component_original = nullptr;
DestroyUIComponentFn g_destroy_ui_component_func = nullptr;
FrameNewSubclassFn g_frame_new_subclass_func = nullptr;
TypedComponentPassthroughFn g_typed_component_passthrough_func = nullptr;
GetFlagPreferenceFn g_get_flag_preference_func = nullptr;
SetFlagPreferenceFn g_set_flag_preference_func = nullptr;
GetStringPreferenceFn g_get_string_preference_func = nullptr;
SetStringPreferenceFn g_set_string_preference_func = nullptr;
GetEnumPreferenceFn g_get_enum_preference_func = nullptr;
SetEnumPreferenceFn g_set_enum_preference_func = nullptr;
GetNumberPreferenceFn g_get_number_preference_func = nullptr;
SetNumberPreferenceFn g_set_number_preference_func = nullptr;
GetGraphicsRendererValueFn g_get_graphics_renderer_value_func = nullptr;
SetGraphicsRendererValueFn g_set_graphics_renderer_value_func = nullptr;
GetGameRendererModeFn g_get_game_renderer_mode_func = nullptr;
SetGameRendererModeFn g_set_game_renderer_mode_func = nullptr;
GetGameRendererMetricFn g_get_game_renderer_metric_func = nullptr;
SetInGameShadowQualityFn g_set_in_game_shadow_quality_func = nullptr;
SetInGameStaticPreferenceFn g_set_in_game_static_preference_func = nullptr;
TriggerTerrainRerenderFn g_trigger_terrain_rerender_func = nullptr;
SetInGameUIScaleFn g_set_in_game_ui_scale_func = nullptr;
SetVolumeFn g_set_volume_func = nullptr;
SetMasterVolumeFn g_set_master_volume_func = nullptr;
ItemImageFramePaintFn g_item_image_frame_paint_func = nullptr;
ItemImageFramePaintFn g_item_image_frame_paint_original = nullptr;
ItemImageFrameCtlMsgProcFn g_item_image_frame_ctl_msg_proc_func = nullptr;
ItemImageFrameCtlMsgProcFn g_item_image_frame_ctl_msg_proc_original = nullptr;
GrModelSetColorFn g_gr_model_set_color_func = nullptr;
GrModelSetMaterialConstantFn g_gr_model_set_material_constant_func = nullptr;
GrMaterialConstantGetIdFn g_gr_material_constant_get_id_func = nullptr;
std::unordered_map<uint32_t, uint32_t> g_item_image_frame_tints;
std::unordered_map<uint32_t, uint32_t> g_item_image_item_tints;
std::unordered_map<uint32_t, uint32_t> g_item_image_frame_by_item_id;
std::atomic<bool> g_item_image_frame_tint_hook_installed = false;
std::atomic<bool> g_item_image_frame_tint_enabled = true;
std::atomic<bool> g_item_image_frame_pop_enabled = false;
std::atomic<float> g_item_image_frame_pop_brightness = 1.35f;
std::atomic<uint64_t> g_item_image_frame_paint_calls = 0;
std::atomic<uint64_t> g_item_image_frame_tint_matches = 0;
std::atomic<uint64_t> g_item_image_frame_model_hits = 0;
std::atomic<uint64_t> g_item_image_frame_color_calls = 0;
std::atomic<uint64_t> g_item_image_frame_icon_model_hits = 0;
std::atomic<uint64_t> g_item_image_frame_icon_color_calls = 0;
std::atomic<uint64_t> g_item_image_frame_icon_constant_calls = 0;
std::atomic<uint32_t> g_item_image_frame_material_constant_id = UINT32_MAX;
std::atomic<uint32_t> g_item_image_frame_last_frame_id = 0;
std::atomic<uintptr_t> g_item_image_frame_last_model = 0;
std::atomic<uintptr_t> g_item_image_frame_last_icon_model = 0;
std::atomic<uint32_t> g_item_image_frame_last_item_id = 0;
std::atomic<uint64_t> g_item_image_frame_item_bindings = 0;
std::atomic<uint32_t> g_item_image_frame_last_bound_item_id = 0;
std::atomic<uint32_t> g_item_image_frame_last_bound_native_frame_id = 0;
uint32_t* g_command_line_number_buffer = nullptr;
GetFlagPreferenceFn g_get_command_line_flag_func = nullptr;
GetStringPreferenceFn g_get_command_line_string_func = nullptr;
GetNumberPreferenceFn g_get_command_line_number_func = nullptr;
SetTooltipFn g_set_tooltip_func = nullptr;
CRITICAL_SECTION g_callback_mutex;
bool g_callback_mutex_initialized = false;
std::unordered_map<UIMessage, std::vector<UIMessageCallbackEntry>> g_ui_message_callbacks;
std::unordered_map<UIMessage, std::vector<FrameUIMessageCallbackEntry>> g_frame_ui_message_callbacks;
std::vector<CreateUIComponentCallbackEntry> g_create_ui_component_callbacks;
bool g_open_links = false;
PY4GW::HookEntry g_open_template_hook;
std::atomic<bool> g_initialized = false;
std::atomic<bool> g_shutting_down = false;
std::atomic<uint32_t> g_active_hooks = 0;
uint32_t g_create_flat_button_dialog_subclass_type = 0;
UIInteractionCallback g_button_frame_callback = nullptr;
UIInteractionCallback g_ctl_button_proc_callback = nullptr;
UIInteractionCallback g_text_button_frame_callback = nullptr;
UIInteractionCallback g_scrollable_frame_callback = nullptr;
UIInteractionCallback g_text_label_frame_callback = nullptr;
UIInteractionCallback g_frame_list_callback = nullptr;
UIInteractionCallback g_dropdown_frame_callback = nullptr;
UIInteractionCallback g_slider_frame_callback = nullptr;
UIInteractionCallback g_slider_frame_wrapper_callback = nullptr;
UIInteractionCallback g_editable_text_frame_callback = nullptr;
UIInteractionCallback g_progress_bar_callback = nullptr;
UIInteractionCallback g_tabs_frame_callback = nullptr;
bool g_typed_component_callbacks_initialized = false;

bool InitializeTypedComponentCallbacks() {
    return TryResolveTypedComponentCallbacks();
}

bool Initialize() {
    CrashContextScope context("startup", "ui", "initialize");
    if (g_initialized) {
        return true;
    }

    PY4GW_ASSERT(PY4GW::Scanner::Initialize());
    PY4GW_ASSERT(PY4GW::Patterns::Initialize());

    PY4GW::HookBase::Initialize();
    if (!Init()) {
        Exit();
        PY4GW::HookBase::Deinitialize();
        return false;
    }

    EnableHooks();
    g_initialized = true;
    return true;
}

void Shutdown() {
    CrashContextScope context("shutdown", "ui", "shutdown");
    if (!g_initialized) {
        return;
    }

    g_shutting_down = true;
    DisableHooks();
    WaitForUiHooksToDrain();
    Exit();
    PY4GW::HookBase::Deinitialize();
    g_shutting_down = false;
    g_initialized = false;
}

}  // namespace GW::ui
