#include "base/error_handling.h"

#include "GW/GuildWars.h"

#include "base/CrashHandler.h"
#include "base/logger.h"
#include "base/memory_manager.h"
#include "base/memory_patcher.h"
#include "GW/agent/agent.h"
#include "GW/agent_recolor/agent_recolor.h"
#include "GW/camera/camera.h"
#include "GW/chat/chat.h"
#include "GW/context/context.h"
#include "GW/dialog/dialog.h"
#include "GW/effects/effects.h"
#include "GW/events/events.h"
#include "GW/friend_list/friend_list.h"
#include "GW/game_thread/game_thread.h"
#include "GW/guild/guild.h"
#include "GW/item/item.h"
#include "GW/map/map.h"
#include "GW/merchant/merchant.h"
#include "GW/native_ui/native_ui.h"
#include "GW/name_obfuscator/name_obfuscator.h"
#include "GW/party/party.h"
#include "GW/player/player.h"
#include "GW/quest/quest.h"
#include "GW/render/render.h"
#include "GW/skillbar/skillbar.h"
#include "GW/stoc/stoc.h"
#include "GW/textures/gw_dat_reader.h"
#include "GW/trade/trade.h"
#include "GW/ui/ui.h"
#include "GW/world_render/world_render.h"

#include <array>

namespace {

struct InitStep {
    const char* module;
    const char* operation;
    bool (*initialize)();
    void (*shutdown)();
};

bool ScanMemoryManager() {
    return PY4GW::MemoryManager::Scan();
}

void ShutdownMemoryManager() {
}

constexpr std::array<InitStep, 27> kInitSteps = {{
    {"game_thread", "initialize", &GW::game_thread::Initialize, &GW::game_thread::Shutdown},
    {"stoc", "initialize", &GW::StoC::Initialize, &GW::StoC::Shutdown},
    {"render", "initialize", &GW::render::Initialize, &GW::render::Shutdown},
    {"ui", "initialize", &GW::ui::Initialize, &GW::ui::Shutdown},
    {"camera", "initialize", &GW::camera::Initialize, &GW::camera::Shutdown},
    {"memory_manager", "scan", &ScanMemoryManager, &ShutdownMemoryManager},
    {"context", "initialize", &GW::Context::Initialize, &GW::Context::Shutdown},
    {"effects", "initialize", &GW::effects::Initialize, &GW::effects::Shutdown},
    {"events", "initialize", &GW::events::Initialize, &GW::events::Shutdown},
    {"friend_list", "initialize", &GW::friend_list::Initialize, &GW::friend_list::Shutdown},
    {"player", "initialize", &GW::player::Initialize, &GW::player::Shutdown},
    {"agent", "initialize", &GW::agent::Initialize, &GW::agent::Shutdown},
    {"quest", "initialize", &GW::quest::Initialize, &GW::quest::Shutdown},
    {"map", "initialize", &GW::map::Initialize, &GW::map::Shutdown},
    {"guild", "initialize", &GW::guild::Initialize, &GW::guild::Shutdown},
    {"chat", "initialize", &GW::chat::Initialize, &GW::chat::Shutdown},
    {"item", "initialize", &GW::item::Initialize, &GW::item::Shutdown},
    {"trade", "initialize", &GW::trade::Initialize, &GW::trade::Shutdown},
    {"merchant", "initialize", &GW::merchant::Initialize, &GW::merchant::Shutdown},
    {"skillbar", "initialize", &GW::skillbar::Initialize, &GW::skillbar::Shutdown},
    {"party", "initialize", &GW::party::Initialize, &GW::party::Shutdown},
    {"name_obfuscator", "initialize", &GW::name_obfuscator::Initialize, &GW::name_obfuscator::Shutdown},
    {"agent_recolor", "initialize", &GW::agent_recolor::Initialize, &GW::agent_recolor::Shutdown},
    {"textures", "initialize", &GW::textures::Initialize, &GW::textures::Shutdown},
    {"dialog", "initialize", &GW::dialog::Initialize, &GW::dialog::Shutdown},
    {"native_ui", "initialize", &GW::native_ui::Initialize, &GW::native_ui::Shutdown},
    {"world_render", "initialize", &GW::world_render::Initialize, &GW::world_render::Shutdown},
}};

void ShutdownInitializedSteps(size_t count) {
    for (size_t i = count; i > 0; --i) {
        const auto& step = kInitSteps[i - 1];
        CrashHandler::SetContext("shutdown", step.module, "shutdown");
        Logger::Instance().LogInfo(std::string("[gw] Shutting down ") + step.module + ".");
        step.shutdown();
    }
}

}

namespace GW {

bool Initialize() {
    size_t initialized_steps = 0;
    for (const auto& step : kInitSteps) {
        CrashHandler::SetContext("startup", step.module, step.operation);
        Logger::Instance().LogInfo(std::string("[gw] Initializing ") + step.module + ".");
        if (!step.initialize()) {
            Logger::Instance().LogError(std::string("[gw] ") + step.module + " initialization failed.");
            ShutdownInitializedSteps(initialized_steps);
            return false;
        }
        ++initialized_steps;
    }

    CrashHandler::SetContext("startup", "memory_patcher", "enable_hooks");
    Logger::Instance().LogInfo("[gw] Enabling memory patcher hooks.");
    PY4GW::MemoryPatcher::EnableHooks();
    CrashHandler::SetContext("runtime", "gw", "initialized");
    Logger::Instance().LogInfo("[gw] Guild Wars initialization complete.");
    return true;
}

void Shutdown() {
    CrashHandler::SetContext("shutdown", "memory_patcher", "disable_hooks");
    Logger::Instance().LogInfo("[gw] Disabling memory patcher hooks.");
    PY4GW::MemoryPatcher::DisableHooks();
    ShutdownInitializedSteps(kInitSteps.size());
    CrashHandler::SetContext("shutdown", "gw", "shutdown_complete");
}

}  // namespace GW
