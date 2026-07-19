#pragma once

#include "base/error_handling.h"

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

// Multi-account ("multibox") shared-memory region.
//
// This is the C++ writer for the cross-account buffer that Python previously
// created and wrote itself (Py4GWCoreLib/GlobalCache/SharedMemory.py, the
// GLOBAL_CACHE.ShMem surface). One fixed contiguous region named
// "Py4GW_Shared_Mem", shared by every boxed client on the machine, holding a
// 64-slot mirror of each account/hero/pet plus the Python-owned coordination
// regions (messaging inbox, HeroAI options, whiteboard intents).
//
// Ownership split after this migration:
//   - C++ (this module) writes the AccountData mirror (game-state reflection) and
//     owns slot allocation, keyed on the stable account-email anchor
//     (PY4GW::System::GetAccountEmail(), which latches once and never changes).
//   - Python still WRITES the coordination regions (Inbox / HeroAIOptions /
//     Intents) because it generates them; C++ never touches those bytes.
//   - Python READS everything.
//
// The structs below MUST stay byte-for-byte identical to the Python ctypes
// mirrors under Py4GWCoreLib/GlobalCache/shared_memory_src/*. #pragma pack(1)
// everywhere; wchar_t is 2 bytes on Windows to match ctypes c_wchar. The
// static_assert block at the bottom is the drift trip-wire: the numbers are
// computed from the Python _pack_=1 layouts and must equal ctypes.sizeof(...) in
// the live interpreter. If a compile fails there, the layout diverged.
//
// NOTE: unlike GW::shared_memory (Plane A, per-process, single-writer), this
// region is a FIXED name with MANY writers and NO header/sequence-lock, because
// the Python AllAccounts layout has no header field and this migration does not
// change the layout. Reads stay as tolerant as they are today.

namespace GW::Context {
struct Skillbar;
struct HeroPartyMember;
struct PetInfo;
}

namespace GW::multibox {

inline constexpr uint32_t kMaxPlayers = 64;
inline constexpr uint32_t kMaxEmailLen = 64;
inline constexpr uint32_t kMaxCharLen = 64;
inline constexpr uint32_t kMaxAvailableChars = 20;
inline constexpr uint32_t kMaxBuffs = 240;
inline constexpr uint32_t kMaxSkills = 8;
inline constexpr uint32_t kMaxAttributes = 46;  // len(Attribute) - MUST equal the Python enum count (was wrongly 43)
inline constexpr uint32_t kMaxTitles = 48;
inline constexpr uint32_t kMaxQuests = 150;
inline constexpr uint32_t kMaxInventoryBagSlots = 20;
inline constexpr uint32_t kMissionBitmapEntries = 25;
inline constexpr uint32_t kSkillBitmapEntries = 108;
inline constexpr uint32_t kMaxIntents = 64;

static_assert(sizeof(wchar_t) == 2, "wchar_t must be 2 bytes to match ctypes c_wchar on Windows");

#pragma pack(push, 1)

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct KeyStruct {
    uint64_t HWND = 0;
    uint32_t EntityType = 0;  // 0=player 1=hero 2=pet 3=npc 4=minion
    uint32_t LocalIndex = 0;
};

struct MapStruct {
    uint32_t MapID = 0;
    int32_t Region = 0;
    uint32_t District = 0;
    int32_t Language = 0;
};

struct SkillStruct {
    uint32_t Id = 0;
    float Recharge = 0.0f;
    float Adrenaline = 0.0f;
};

struct SkillbarStruct {
    uint32_t CastingSkillID = 0;
    SkillStruct Skills[kMaxSkills] = {};
};

struct AttributeUnitStruct {
    uint32_t Id = 0;
    uint32_t Value = 0;
    uint32_t BaseValue = 0;
};

struct AttributesStruct {
    AttributeUnitStruct Attributes[kMaxAttributes] = {};
};

struct BuffUnitStruct {
    uint32_t SkillId = 0;
    float Duration = 0.0f;
    float Remaining = 0.0f;
    uint32_t TargetAgentID = 0;
    int32_t Type = 0;
};

struct BuffStruct {
    BuffUnitStruct Buffs[kMaxBuffs] = {};
};

struct HealthStruct {
    float Current = 0.0f;
    float Max = 0.0f;
    float Regen = 0.0f;
    int32_t Pips = 0;
};

struct EnergyStruct {
    float Current = 0.0f;
    int32_t Max = 0;  // NOTE: c_int in Python (Health.Max is c_float; Energy.Max is c_int)
    float Regen = 0.0f;
    int32_t Pips = 0;
};

struct AgentDataStruct {
    wchar_t CharacterName[kMaxCharLen] = {};

    uint32_t AgentID = 0;
    uint32_t UUID[4] = {};
    uint32_t OwnerAgentID = 0;
    uint32_t HeroID = 0;
    uint32_t TargetID = 0;
    uint32_t ObservingID = 0;
    uint32_t PlayerNumber = 0;
    uint32_t LoginNumber = 0;

    MapStruct Map = {};
    SkillbarStruct Skillbar = {};
    AttributesStruct Attributes = {};
    BuffStruct Buffs = {};

    HealthStruct Health = {};
    EnergyStruct Energy = {};
    float Overcast = 0.0f;
    uint32_t Level = 0;
    uint32_t Profession[2] = {};  // primary, secondary
    uint32_t Morale = 0;
    Vec3f Pos = {};
    int32_t ZPlane = 0;
    float RotationAngle = 0.0f;
    Vec2f Velocity = {};

    uint32_t DaggerStatus = 0;
    uint32_t WeaponType = 0;
    uint32_t WeaponItemType = 0;
    uint32_t OffhandItemType = 0;
    float WeaponAttackSpeed = 0.0f;
    float AttackSpeedModifier = 0.0f;
    uint32_t EffectsMask = 0;
    uint32_t VisualEffectsMask = 0;
    uint32_t TypeMap = 0;
    uint32_t ModelState = 0;
    float AnimationSpeed = 0.0f;
    uint32_t AnimationCode = 0;
    uint32_t AnimationID = 0;
};

struct InventorySlotStruct {
    uint32_t BagID = 0;
    uint32_t Slot = 0;
    uint32_t ModelID = 0;
    uint32_t Quantity = 0;
};

struct InventoryBagStruct {
    uint32_t BagID = 0;
    uint32_t Size = 0;
    InventorySlotStruct Slots[kMaxInventoryBagSlots] = {};
};

struct InventoryBagsStruct {
    InventoryBagStruct Backpack = {};
    InventoryBagStruct BeltPouch = {};
    InventoryBagStruct Bag1 = {};
    InventoryBagStruct Bag2 = {};
};

struct AgentPartyStruct {
    bool IsTicked = false;
    uint32_t PartyID = 0;
    uint32_t PartyPosition = 0;
    bool IsPartyLeader = false;
};

struct RankStruct {
    uint32_t Rank = 0;
    uint32_t Rating = 0;
    uint32_t QualifierPoints = 0;
    uint32_t Wins = 0;
    uint32_t Losses = 0;
    uint32_t TournamentRewardPoints = 0;
};

struct FactionUnitStruct {
    uint32_t Current = 0;
    uint32_t TotalEarned = 0;
    uint32_t Max = 0;
};

struct FactionStruct {
    FactionUnitStruct Kurzick = {};
    FactionUnitStruct Luxon = {};
    FactionUnitStruct Imperial = {};
    FactionUnitStruct Balthazar = {};
};

struct TitleUnitStruct {
    uint32_t TitleID = 0;
    uint32_t CurrentPoints = 0;
};

struct TitlesStruct {
    uint32_t ActiveTitleID = 0;
    TitleUnitStruct Titles[kMaxTitles] = {};
};

struct QuestStruct {
    uint32_t QuestID = 0;
    bool IsCompleted = false;
    float MarkerX = 0.0f;
    float MarkerY = 0.0f;
};

struct QuestLogStruct {
    uint32_t ActiveQuestID = 0;
    QuestStruct Quests[kMaxQuests] = {};
};

struct ExperienceStruct {
    uint32_t Level = 0;
    uint32_t Experience = 0;
    float ProgressPct = 0.0f;
    uint32_t CurrentSkillPoints = 0;
    uint32_t TotalEarnedSkillPoints = 0;
};

struct MissionDataStruct {
    uint32_t NormalModeCompleted[kMissionBitmapEntries] = {};
    uint32_t NormalModeBonus[kMissionBitmapEntries] = {};
    uint32_t HardModeCompleted[kMissionBitmapEntries] = {};
    uint32_t HardModeBonus[kMissionBitmapEntries] = {};
};

struct UnlockedSkillsStruct {
    uint32_t Skills[kSkillBitmapEntries] = {};
};

struct AvailableCharacterUnitStruct {
    wchar_t Name[kMaxCharLen] = {};
    uint32_t Level = 0;
    bool IsPvP = false;
    uint32_t MapID = 0;
    uint32_t Professions[2] = {};  // primary, secondary
    uint32_t CampaignID = 0;
};

struct AvailableCharacterStruct {
    AvailableCharacterUnitStruct Characters[kMaxAvailableChars] = {};
};

struct AccountStruct {
    KeyStruct Key = {};
    wchar_t AccountEmail[kMaxEmailLen] = {};
    wchar_t AccountName[kMaxCharLen] = {};

    AgentDataStruct AgentData = {};
    InventoryBagsStruct InventoryBags = {};
    AgentPartyStruct AgentPartyData = {};

    RankStruct RankData = {};
    FactionStruct FactionData = {};
    TitlesStruct TitlesData = {};
    QuestLogStruct QuestLog = {};
    ExperienceStruct ExperienceData = {};
    MissionDataStruct MissionData = {};
    UnlockedSkillsStruct UnlockedSkills = {};
    AvailableCharacterStruct AvailableCharacters = {};

    uint32_t SlotNumber = 0;
    bool IsSlotActive = false;
    bool IsAccount = false;
    bool IsHero = false;
    bool IsPet = false;
    bool IsNPC = false;
    bool IsIsolated = false;
    uint32_t IsolationGroupID = 0;
    bool InAggro = false;
    uint64_t InAggroTick64 = 0;
    uint32_t LastUpdated = 0;
};

// --- Coordination regions (Python-written; mirrored here for layout only) ------

struct SharedMessageStruct {
    wchar_t SenderEmail[kMaxEmailLen] = {};
    wchar_t ReceiverEmail[kMaxEmailLen] = {};
    uint32_t Command = 0;
    float Params[4] = {};
    wchar_t ExtraData[4][kMaxCharLen] = {};
    bool Active = false;
    bool Running = false;
    uint32_t Timestamp = 0;
};

struct HeroAIOptionStruct {
    bool Following = false;
    bool Avoidance = false;
    bool Looting = false;
    bool Targeting = false;
    bool Combat = false;
    bool Skills[kMaxSkills] = {};
    bool IsFlagged = false;
    float FlagPosX = 0.0f;
    float FlagPosY = 0.0f;
    float FlagFacingAngle = 0.0f;
    Vec3f FollowPos = {};
    float FollowMoveThreshold = 0.0f;
    float FollowMoveThresholdCombat = 0.0f;
    bool LeaderFollowReady = false;
    Vec2f FollowOffset = {};
    Vec2f FlagPos = {};
    Vec2f AllFlag = {};
};

struct IntentStruct {
    wchar_t OwnerEmail[kMaxEmailLen] = {};
    uint32_t KindID = 0;
    uint32_t LockMode = 0;
    uint32_t ReentryPolicy = 0;
    uint32_t ClaimStrength = 0;
    uint32_t MaxHolders = 0;
    uint32_t SkillID = 0;
    uint32_t TargetAgentID = 0;
    uint32_t IsolationGroupID = 0;
    uint32_t PostedAtTick = 0;
    uint32_t ExpiresAtTick = 0;
    bool Active = false;
};

struct AllAccounts {
    KeyStruct Keys[kMaxPlayers] = {};
    AccountStruct AccountData[kMaxPlayers] = {};
    SharedMessageStruct Inbox[kMaxPlayers] = {};
    HeroAIOptionStruct HeroAIOptions[kMaxPlayers] = {};
    IntentStruct Intents[kMaxIntents] = {};
};

#pragma pack(pop)

// --- Layout drift trip-wire ----------------------------------------------------
// Values computed from the Python _pack_=1 mirrors. Must equal ctypes.sizeof(...)
// in the live interpreter. See docs/multibox_shmem_cpp_writer_plan.md.
static_assert(sizeof(KeyStruct) == 16, "KeyStruct layout drift");
static_assert(sizeof(MapStruct) == 16, "MapStruct layout drift");
static_assert(sizeof(HealthStruct) == 16, "HealthStruct layout drift");
static_assert(sizeof(EnergyStruct) == 16, "EnergyStruct layout drift");
static_assert(sizeof(AgentPartyStruct) == 10, "AgentPartyStruct layout drift");
static_assert(sizeof(SkillbarStruct) == 100, "SkillbarStruct layout drift");
static_assert(sizeof(AttributesStruct) == 552, "AttributesStruct layout drift");  // 46 * 12
static_assert(sizeof(BuffStruct) == 4800, "BuffStruct layout drift");
static_assert(sizeof(InventoryBagStruct) == 328, "InventoryBagStruct layout drift");
static_assert(sizeof(InventoryBagsStruct) == 1312, "InventoryBagsStruct layout drift");
static_assert(sizeof(RankStruct) == 24, "RankStruct layout drift");
static_assert(sizeof(FactionStruct) == 48, "FactionStruct layout drift");
static_assert(sizeof(TitlesStruct) == 388, "TitlesStruct layout drift");
static_assert(sizeof(QuestLogStruct) == 1954, "QuestLogStruct layout drift");
static_assert(sizeof(ExperienceStruct) == 20, "ExperienceStruct layout drift");
static_assert(sizeof(MissionDataStruct) == 400, "MissionDataStruct layout drift");
static_assert(sizeof(UnlockedSkillsStruct) == 432, "UnlockedSkillsStruct layout drift");
static_assert(sizeof(AvailableCharacterStruct) == 2980, "AvailableCharacterStruct layout drift");
static_assert(sizeof(AgentDataStruct) == 5772, "AgentDataStruct layout drift");  // live Python ctypes.sizeof
static_assert(sizeof(AccountStruct) == 13639, "AccountStruct layout drift");  // live Python ctypes.sizeof
static_assert(sizeof(SharedMessageStruct) == 794, "SharedMessageStruct layout drift");
static_assert(sizeof(HeroAIOptionStruct) == 71, "HeroAIOptionStruct layout drift");
static_assert(sizeof(IntentStruct) == 169, "IntentStruct layout drift");
static_assert(sizeof(AllAccounts) == 940096, "AllAccounts layout drift");  // live Python ctypes.sizeof (matches the runtime ValueError target)

// -------------------------------------------------------------------------------

class Manager {
public:
    Manager() = default;
    ~Manager();

    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;

    // Create the shared region if it does not exist, otherwise attach to the
    // existing one. Only the creator zeroes the whole region (late joiners must
    // not clobber slots owned by other clients). Idempotent after success.
    bool CreateOrOpen(const std::wstring& name);
    void Destroy();

    // Publish one frame: write this client's own account + heroes + pets into
    // their slots (claiming/finding by the stable email anchor), zeroing an
    // entity's payload when its map gates are not met. Never touches the
    // Python-owned coordination regions (Inbox / HeroAIOptions / Intents).
    bool Update();

    bool IsValid() const;
    void* Data() const;
    size_t Size() const;
    const std::wstring& Name() const;
    bool IsCreator() const { return is_creator_; }

    AllAccounts* View() const;

private:
    // Slot dispatch (keyed on the stable email anchor). Mirror of the Python
    // AllAccounts find/claim methods, own-entity scope only.
    int FindAccountSlot(const std::string& email) const;
    int FindEmptyOrExpiredSlot(bool allow_expired_reclaim) const;
    bool IsSlotExpired(int index, uint64_t now) const;

    int FindOrClaimAccountSlot(const std::string& email);

    // Per-entity fill (ports Py4GWCoreLib .../AccountStruct.from_context et al.).
    // When map gates are unmet these zero the payload but keep slot ownership.
    void UpdateOwnAccount();
    void FillAccountPayload(AccountStruct& slot, const std::string& email, int slot_index, bool gates_ok);
    void FillAgentData(AgentDataStruct& out, uint32_t agent_id, const GW::Context::Skillbar* skillbar);
    void ZeroAccountPayload(AccountStruct& slot);

    // Hero/pet child slots — map-instance entities owned by this account,
    // recycled across map transitions (ports SetHeroesData / SetPetData).
    int FindChildSlot(const std::string& email, uint32_t entity_type, uint32_t local_id) const;
    int FindOrClaimChildSlot(const std::string& email, uint32_t entity_type, uint32_t local_id);
    void UpdateOwnHeroesAndPets(const std::string& email);
    void FillHeroPayload(AccountStruct& slot, const std::string& email, int slot_index,
                         const GW::Context::HeroPartyMember& hero, uint32_t hero_index, uint32_t owner_agent_id);
    void FillPetPayload(AccountStruct& slot, const std::string& email, int slot_index,
                        const GW::Context::PetInfo& pet, uint32_t owner_agent_id);

    static void WriteWideField(wchar_t* dst, size_t cap, const std::wstring& src);
    static void WriteWideField(wchar_t* dst, size_t cap, const std::string& src);

    HANDLE mapping_handle_ = nullptr;
    void* view_ = nullptr;
    size_t total_size_ = 0;
    std::wstring name_;
    bool is_creator_ = false;
};

Manager& RuntimeManager();

}  // namespace GW::multibox
