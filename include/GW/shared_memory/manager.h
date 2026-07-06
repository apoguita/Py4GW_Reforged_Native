#pragma once

#include "base/error_handling.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

// Shared-memory runtime region.
//
// This is a faithful port of the legacy Py4GW shared memory: a single fixed
// contiguous region laid out as
//
//     [ SharedMemoryHeader ][ AgentArray_SHMemStruct ][ Pointers_SHMemStruct ]
//
// published every frame under a sequence-lock. The Python reader
// (Py4GWCoreLib/native_src/ShMem/SysShaMem.py) mirrors these structs byte for
// byte and locates the two payloads at fixed offsets derived from struct sizes.
//
// The Pointers block is the single, unified pointer directory (it supersedes the
// legacy standalone PyPointers module): every top-level game context address is
// published here, and Python materializes each via ctypes at the shared address
// (the DLL and the interpreter share the game's address space).
//
// Layout is fixed and must stay in lockstep with the Python mirrors. Do not add
// descriptor tables, per-segment directories, or reorder fields without updating
// the Python structs in the same change.
namespace GW::shared_memory {

inline constexpr uint32_t kAgentArrayMaxSize = 300;
inline constexpr uint32_t kSharedMemoryVersion = 1;

// 24 bytes, natural alignment (matches the Python _pack_=1 mirror: no padding is
// introduced because window_handle is already 8-aligned at offset 16).
struct SharedMemoryHeader {
    uint32_t version = kSharedMemoryVersion;
    uint32_t total_size = 0;
    uint32_t sequence = 0;
    uint32_t process_id = 0;
    uint64_t window_handle = 0;
};

#pragma pack(push, 1)
struct Agent_SHMemStruct {
    uintptr_t ptr = 0;
    uint32_t agent_id = 0;
};

struct AgentRef_SHMemStruct {
    uint32_t agent_id = 0;
    uint32_t index = 0;
};

struct AgentRefArray_SHMemStruct {
    uint32_t count = 0;
    AgentRef_SHMemStruct entries[kAgentArrayMaxSize] = {};
};

struct AgentArray_SHMemStruct {
    uint32_t max_size = kAgentArrayMaxSize;
    uint32_t AgentArrayCount = 0;
    Agent_SHMemStruct AgentArray[kAgentArrayMaxSize] = {};
    AgentRefArray_SHMemStruct AllArray = {};
    AgentRefArray_SHMemStruct AllyArray = {};
    AgentRefArray_SHMemStruct NeutralArray = {};
    AgentRefArray_SHMemStruct EnemyArray = {};
    AgentRefArray_SHMemStruct SpiritPetArray = {};
    AgentRefArray_SHMemStruct MinionArray = {};
    AgentRefArray_SHMemStruct NPCMinipetArray = {};
    AgentRefArray_SHMemStruct LivingArray = {};
    AgentRefArray_SHMemStruct ItemArray = {};
    AgentRefArray_SHMemStruct OwnedItemArray = {};
    AgentRefArray_SHMemStruct GadgetArray = {};
    AgentRefArray_SHMemStruct DeadAllyArray = {};
    AgentRefArray_SHMemStruct DeadEnemyArray = {};
};

// The unified pointer directory (merged PyPointers). Every field is a game
// context address; 0 means "not resolved this frame".
struct Pointers_SHMemStruct {
    uintptr_t MissionMapContext = 0;
    uintptr_t WorldMapContext = 0;
    uintptr_t GameplayContext = 0;
    uintptr_t InstanceInfo = 0;
    uintptr_t MapContext = 0;
    uintptr_t GameContext = 0;
    uintptr_t PreGameContext = 0;
    uintptr_t WorldContext = 0;
    uintptr_t CharContext = 0;
    uintptr_t AgentContext = 0;
    uintptr_t CinematicContext = 0;
    uintptr_t GuildContext = 0;
    uintptr_t AvailableCharacters = 0;
    uintptr_t PartyContext = 0;
    uintptr_t ServerRegionContext = 0;
    uintptr_t Camera = 0;
};
#pragma pack(pop)

class Manager {
public:
    Manager() = default;
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    // Create the fixed runtime region (sized to hold the header + agent array +
    // pointers block). Idempotent: a second call after a successful Create is a
    // no-op that returns true.
    bool Create(const std::wstring& name);
    void Destroy();

    // Publish one frame: bump the sequence odd, refill the agent array and the
    // pointer directory, bump the sequence even. Safe to call every frame.
    bool Update();

    bool IsValid() const;
    void* Data() const;
    size_t Size() const;
    const std::wstring& Name() const;
    SharedMemoryHeader* Header() const;

    static std::wstring BuildName(const wchar_t* prefix, DWORD process_id, HWND window_handle = nullptr);

    // Fixed-layout offsets (must match the Python reader's offset math).
    static constexpr size_t AgentArrayPayloadOffset() {
        return sizeof(SharedMemoryHeader);
    }
    static constexpr size_t PointersPayloadOffset() {
        return AgentArrayPayloadOffset() + sizeof(AgentArray_SHMemStruct);
    }
    static constexpr size_t RuntimeRegionSize() {
        return PointersPayloadOffset() + sizeof(Pointers_SHMemStruct);
    }

private:
    // Sequence-lock helpers: BeginWrite makes the sequence odd (write in
    // progress), EndWrite makes it even (quiescent). Readers accept a snapshot
    // only when the sequence is even and unchanged across the read.
    void BeginWrite() const;
    void EndWrite() const;

    void* PayloadData(size_t offset) const;
    template <typename T>
    T* PayloadAs(size_t offset) const {
        return static_cast<T*>(PayloadData(offset));
    }

    AgentArray_SHMemStruct* AgentArraySMStruct() const;
    Pointers_SHMemStruct* PointersSMStruct() const;
    bool UpdateAgentArrayRegion();
    bool UpdatePointersRegion();

    HANDLE mapping_handle_ = nullptr;
    void* view_ = nullptr;
    size_t total_size_ = 0;
    std::wstring name_;
};

Manager& RuntimeManager();

}  // namespace GW::shared_memory
