#include "base/error_handling.h"

#include "GW/shared_memory/manager.h"

#include "GW/common/constants/constants.h"
#include "GW/context/agent.h"
#include "GW/context/context.h"
#include "GW/context/game.h"
#include "GW/context/map.h"
#include "GW/context/pregame.h"
#include "GW/map/map.h"
#include "GW/render/render.h"

#include <cstdio>

// Faithful port of the legacy Py4GW SharedMemoryManager: one fixed contiguous
// region [ Header ][ AgentArray ][ Pointers ] published each frame under a
// sequence-lock. Populated from the reforged GW accessors; read unchanged by
// Py4GWCoreLib/native_src/ShMem/SysShaMem.py.
namespace GW::shared_memory {

namespace {
Manager g_runtime_manager;
}  // namespace

Manager::~Manager() {
    Destroy();
}

bool Manager::Create(const std::wstring& name) {
    if (IsValid()) {
        return true;
    }
    if (name.empty()) {
        return false;
    }

    const size_t total_size = RuntimeRegionSize();
    const DWORD high = static_cast<DWORD>((static_cast<unsigned long long>(total_size) >> 32) & 0xFFFFFFFFull);
    const DWORD low = static_cast<DWORD>(static_cast<unsigned long long>(total_size) & 0xFFFFFFFFull);

    HANDLE mapping = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, high, low, name.c_str());
    if (!mapping) {
        return false;
    }

    void* view = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
    if (!view) {
        ::CloseHandle(mapping);
        return false;
    }

    mapping_handle_ = mapping;
    view_ = view;
    total_size_ = total_size;
    name_ = name;

    ZeroMemory(view_, total_size_);

    auto* header = Header();
    header->version = kSharedMemoryVersion;
    header->total_size = static_cast<uint32_t>(total_size_);
    header->sequence = 0;
    header->process_id = ::GetCurrentProcessId();
    header->window_handle = reinterpret_cast<uint64_t>(GW::render::GetWindowHandle());

    return true;
}

void Manager::Destroy() {
    if (view_) {
        ::UnmapViewOfFile(view_);
        view_ = nullptr;
    }
    if (mapping_handle_) {
        ::CloseHandle(mapping_handle_);
        mapping_handle_ = nullptr;
    }
    total_size_ = 0;
    name_.clear();
}

bool Manager::IsValid() const {
    return mapping_handle_ != nullptr && view_ != nullptr;
}

void* Manager::Data() const {
    return view_;
}

size_t Manager::Size() const {
    return total_size_;
}

const std::wstring& Manager::Name() const {
    return name_;
}

SharedMemoryHeader* Manager::Header() const {
    if (!view_) {
        return nullptr;
    }
    return static_cast<SharedMemoryHeader*>(view_);
}

void Manager::BeginWrite() const {
    if (auto* header = Header()) {
        ++header->sequence;  // -> odd: write in progress
    }
}

void Manager::EndWrite() const {
    if (auto* header = Header()) {
        ++header->sequence;  // -> even: quiescent
    }
}

void* Manager::PayloadData(size_t offset) const {
    if (!view_ || offset >= total_size_) {
        return nullptr;
    }
    return static_cast<uint8_t*>(view_) + offset;
}

AgentArray_SHMemStruct* Manager::AgentArraySMStruct() const {
    return PayloadAs<AgentArray_SHMemStruct>(AgentArrayPayloadOffset());
}

Pointers_SHMemStruct* Manager::PointersSMStruct() const {
    return PayloadAs<Pointers_SHMemStruct>(PointersPayloadOffset());
}

std::wstring Manager::BuildName(const wchar_t* prefix, DWORD process_id, HWND window_handle) {
    wchar_t buffer[160] = {0};
    swprintf_s(
        buffer,
        L"%ls_%lu_%p",
        prefix ? prefix : L"Py4GW_SharedMemory_PID_",
        static_cast<unsigned long>(process_id),
        window_handle);
    return std::wstring(buffer);
}

bool Manager::UpdateAgentArrayRegion() {
    AgentArray_SHMemStruct* payload = AgentArraySMStruct();
    if (!payload) {
        return false;
    }

    const auto instance_type = GW::map::GetInstanceType();
    const bool is_map_ready = GW::map::GetIsMapLoaded() && !GW::map::GetIsObserving() &&
                              instance_type != GW::Constants::InstanceType::Loading;

    auto* agents = GW::Context::GetAgentArray();
    auto* agent_context = GW::Context::GetAgentContext();

    ZeroMemory(payload, sizeof(*payload));
    payload->max_size = kAgentArrayMaxSize;

    if (!agents || !agent_context) {
        return false;
    }
    if (!is_map_ready) {
        return false;
    }

    auto push_ref = [](AgentRefArray_SHMemStruct& arr, uint32_t agent_id, uint32_t index) {
        if (arr.count >= kAgentArrayMaxSize) {
            return;
        }
        AgentRef_SHMemStruct& entry = arr.entries[arr.count++];
        entry.agent_id = agent_id;
        entry.index = index;
    };

    for (const GW::Context::Agent* agent : *agents) {
        if (!agent) {
            continue;
        }

        const uint32_t id = agent->agent_id;
        if (!id) {
            continue;
        }

        // Stale-pointer gate (matches the legacy FilterAgents guard).
        if (!(agent_context->agent_movement.size() > id && agent_context->agent_movement[id])) {
            continue;
        }

        if (payload->AgentArrayCount >= kAgentArrayMaxSize) {
            break;
        }

        const uint32_t slot = payload->AgentArrayCount++;
        Agent_SHMemStruct& out = payload->AgentArray[slot];
        out.ptr = reinterpret_cast<uintptr_t>(agent);
        out.agent_id = id;
        push_ref(payload->AllArray, id, slot);

        if (agent->GetIsGadgetType()) {
            push_ref(payload->GadgetArray, id, slot);
            continue;
        }

        if (agent->GetIsItemType()) {
            const auto* item = agent->GetAsAgentItem();
            if (!item) {
                continue;
            }
            push_ref(payload->ItemArray, id, slot);
            if (item->owner != 0) {
                push_ref(payload->OwnedItemArray, id, slot);
            }
            continue;
        }

        const auto* living = agent->GetAsAgentLiving();
        if (!living) {
            continue;
        }

        push_ref(payload->LivingArray, id, slot);

        switch (living->allegiance) {
        case GW::Constants::Allegiance::Ally_NonAttackable:
            push_ref(payload->AllyArray, id, slot);
            if (living->GetIsDead()) {
                push_ref(payload->DeadAllyArray, id, slot);
            }
            break;
        case GW::Constants::Allegiance::Neutral:
            push_ref(payload->NeutralArray, id, slot);
            break;
        case GW::Constants::Allegiance::Enemy:
            push_ref(payload->EnemyArray, id, slot);
            if (living->GetIsDead()) {
                push_ref(payload->DeadEnemyArray, id, slot);
            }
            break;
        case GW::Constants::Allegiance::Spirit_Pet:
            push_ref(payload->SpiritPetArray, id, slot);
            break;
        case GW::Constants::Allegiance::Minion:
            push_ref(payload->MinionArray, id, slot);
            break;
        case GW::Constants::Allegiance::Npc_Minipet:
            push_ref(payload->NPCMinipetArray, id, slot);
            break;
        default:
            break;
        }
    }

    return true;
}

bool Manager::UpdatePointersRegion() {
    Pointers_SHMemStruct* payload = PointersSMStruct();
    if (!payload) {
        return false;
    }

    ZeroMemory(payload, sizeof(*payload));

    payload->MissionMapContext = reinterpret_cast<uintptr_t>(GW::Context::GetMissionMapContext());
    payload->WorldMapContext = reinterpret_cast<uintptr_t>(GW::Context::GetWorldMapContext());
    payload->GameplayContext = reinterpret_cast<uintptr_t>(GW::Context::GetGameplayContext());
    payload->InstanceInfo = GW::Context::GetInstanceInfoPtr();
    payload->MapContext = reinterpret_cast<uintptr_t>(GW::Context::GetMapContext());

    auto* game = GW::Context::GetGameContext();
    payload->GameContext = reinterpret_cast<uintptr_t>(game);

    auto* pregame = GW::Context::GetPreGameContext();
    payload->PreGameContext = reinterpret_cast<uintptr_t>(pregame);

    payload->WorldContext = reinterpret_cast<uintptr_t>(GW::Context::GetWorldContext());
    payload->CharContext = reinterpret_cast<uintptr_t>(GW::Context::GetCharContext());
    payload->AgentContext = reinterpret_cast<uintptr_t>(GW::Context::GetAgentContext());
    // Legacy sourced this from GetGameContext()->cinematic; there is no dedicated accessor.
    payload->CinematicContext = reinterpret_cast<uintptr_t>(game ? game->cinematic : nullptr);
    payload->GuildContext = reinterpret_cast<uintptr_t>(GW::Context::GetGuildContext());
    // Deviation: the reforged native has no standalone AvailableCharacters pointer accessor.
    // The login-character buffer lives on PreGameContext; publish its address.
    payload->AvailableCharacters = reinterpret_cast<uintptr_t>(pregame ? pregame->chars_buffer : nullptr);
    payload->PartyContext = reinterpret_cast<uintptr_t>(GW::Context::GetPartyContext());
    payload->ServerRegionContext = GW::map::GetServerRegionPtr();
    payload->Camera = reinterpret_cast<uintptr_t>(GW::Context::GetCamera());

    return true;
}

bool Manager::Update() {
    if (!IsValid()) {
        return false;
    }
    BeginWrite();
    UpdateAgentArrayRegion();
    UpdatePointersRegion();
    EndWrite();
    return true;
}

Manager& RuntimeManager() {
    return g_runtime_manager;
}

}  // namespace GW::shared_memory
