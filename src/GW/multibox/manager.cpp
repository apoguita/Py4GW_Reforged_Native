#include "GW/multibox/manager.h"

#include "system/system.h"

#include "GW/agent/agent.h"
#include "GW/effects/effects.h"
#include "GW/map/map.h"
#include "GW/party/party.h"
#include "GW/player/player.h"
#include "GW/skillbar/skillbar.h"

#include "GW/context/account.h"
#include "GW/context/agent.h"
#include "GW/context/attribute.h"
#include "GW/context/character.h"
#include "GW/context/context.h"
#include "GW/context/hero.h"
#include "GW/context/item.h"
#include "GW/context/party.h"
#include "GW/context/quest.h"
#include "GW/context/skill.h"
#include "GW/context/title.h"
#include "GW/context/world.h"

#include "GW/common/constants/constants.h"

#include <cstring>

// C++ writer for the multi-account ("multibox") shared-memory region. Ported from
// the legacy Python writer (Py4GWCoreLib/GlobalCache/SharedMemory.py +
// shared_memory_src/*). Each client writes ONLY its own account plus the heroes
// and pets it owns, claiming slots keyed on the stable account-email anchor. The
// Python side is now read-only and still writes the coordination regions
// (Inbox / HeroAIOptions / Intents), which this module never touches.
namespace GW::multibox {

namespace {
Manager g_runtime_manager;

// Matches SHMEM_SUBSCRIBE_TIMEOUT_MILLISECONDS in the Python Globals.
constexpr uint32_t kSubscribeTimeoutMs = 5000;

// LastUpdated is a uint32 in the layout; the Python reader's liveness check is
// (PySystem.get_tick_count64() - LastUpdated) < 5000. PySystem.get_tick_count64
// returns the FRAME-COHERENT timestamp (PY4GW::System::GetTickCount64, stamped
// once per frame), NOT the raw system tick - so the writer MUST stamp from the
// same clock. Using raw ::GetTickCount64() here makes (frame_ts - LastUpdated)
// underflow to a huge unsigned value whenever the raw tick has advanced past the
// frame stamp, which reads as "expired" and flickers the slot in and out.
uint32_t NowTick32() {
    return static_cast<uint32_t>(PY4GW::System::GetTickCount64());
}
}  // namespace

Manager::~Manager() {
    Destroy();
}

void Manager::WriteWideField(wchar_t* dst, size_t cap, const std::wstring& src) {
    if (!dst || cap == 0) {
        return;
    }
    const size_t n = src.size() < (cap - 1) ? src.size() : (cap - 1);
    std::wmemset(dst, 0, cap);
    if (n) {
        std::wmemcpy(dst, src.c_str(), n);
    }
}

void Manager::WriteWideField(wchar_t* dst, size_t cap, const std::string& src) {
    if (!dst || cap == 0) {
        return;
    }
    std::wmemset(dst, 0, cap);
    const size_t n = src.size() < (cap - 1) ? src.size() : (cap - 1);
    for (size_t i = 0; i < n; ++i) {
        dst[i] = static_cast<wchar_t>(static_cast<unsigned char>(src[i]));
    }
}

// --- lifecycle ---------------------------------------------------------------

bool Manager::CreateOrOpen(const std::wstring& name) {
    if (IsValid()) {
        return true;
    }
    if (name.empty()) {
        return false;
    }

    const size_t total_size = sizeof(AllAccounts);
    const DWORD high = static_cast<DWORD>((static_cast<unsigned long long>(total_size) >> 32) & 0xFFFFFFFFull);
    const DWORD low = static_cast<DWORD>(static_cast<unsigned long long>(total_size) & 0xFFFFFFFFull);

    // CreateFileMapping returns the existing object if another client already made
    // it; ERROR_ALREADY_EXISTS distinguishes creator from late joiner.
    HANDLE mapping = ::CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, high, low, name.c_str());
    if (!mapping) {
        return false;
    }
    const bool already_existed = (::GetLastError() == ERROR_ALREADY_EXISTS);

    void* view = ::MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
    if (!view) {
        ::CloseHandle(mapping);
        return false;
    }

    mapping_handle_ = mapping;
    view_ = view;
    total_size_ = total_size;
    name_ = name;
    is_creator_ = !already_existed;

    // Creator-only, ONE TIME. HeroAIOptions and messaging (Inbox) are separate
    // Python-owned systems; the ONLY moment C++ touches HeroAIOptions is here, to
    // seed its non-zero "enabled" defaults on a brand-new buffer (legacy
    // ResetHeroAIData did this). After create, C++ NEVER intervenes with the
    // options or messaging again — runtime Update() writes AccountData only.
    //
    // Inbox / Intents default to their zero (empty) state, which a fresh
    // pagefile-backed mapping already provides, so C++ leaves them untouched.
    if (is_creator_) {
        AllAccounts* all = View();
        ZeroMemory(all->Keys, sizeof(all->Keys));
        ZeroMemory(all->AccountData, sizeof(all->AccountData));

        for (uint32_t i = 0; i < kMaxPlayers; ++i) {
            HeroAIOptionStruct& o = all->HeroAIOptions[i];
            o.Following = true;
            o.Avoidance = true;
            o.Looting = true;
            o.Targeting = true;
            o.Combat = true;
            for (uint32_t s = 0; s < kMaxSkills; ++s) {
                o.Skills[s] = true;
            }
            o.FollowMoveThreshold = -1.0f;
            o.FollowMoveThresholdCombat = -1.0f;
        }
    }

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
    is_creator_ = false;
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

AllAccounts* Manager::View() const {
    return static_cast<AllAccounts*>(view_);
}

// --- slot dispatch (keyed on the stable email anchor) ------------------------

// Compare a slot's stored wide AccountEmail against the anchor (ASCII).
static bool SlotEmailEquals(const wchar_t* stored, size_t cap, const std::string& email) {
    size_t i = 0;
    for (; i < cap; ++i) {
        const wchar_t c = stored[i];
        if (c == 0) {
            break;
        }
        if (i >= email.size()) {
            return false;
        }
        if (static_cast<wchar_t>(static_cast<unsigned char>(email[i])) != c) {
            return false;
        }
    }
    return i == email.size();
}

bool Manager::IsSlotExpired(int index, uint64_t now) const {
    AllAccounts* view = View();
    const AccountStruct& slot = view->AccountData[index];
    if (!slot.IsSlotActive) {
        return false;
    }
    return (static_cast<uint32_t>(now) - slot.LastUpdated) >= kSubscribeTimeoutMs;
}

int Manager::FindAccountSlot(const std::string& email) const {
    if (email.empty()) {
        return -1;
    }
    AllAccounts* view = View();
    for (uint32_t i = 0; i < kMaxPlayers; ++i) {
        const AccountStruct& slot = view->AccountData[i];
        if (slot.IsAccount && SlotEmailEquals(slot.AccountEmail, kMaxEmailLen, email)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Manager::FindEmptyOrExpiredSlot(bool allow_expired_reclaim) const {
    AllAccounts* view = View();
    for (uint32_t i = 0; i < kMaxPlayers; ++i) {
        if (!view->AccountData[i].IsSlotActive) {
            return static_cast<int>(i);
        }
    }
    if (!allow_expired_reclaim) {
        return -1;
    }
    // Same frame-coherent clock the LastUpdated stamps use (see NowTick32).
    const uint64_t now = PY4GW::System::GetTickCount64();
    for (uint32_t i = 0; i < kMaxPlayers; ++i) {
        if (IsSlotExpired(static_cast<int>(i), now)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// Resolve this client's own account slot, claiming one if we do not own it yet.
// NOTE (sturdiness): the claim below is not yet atomic against other writers.
// Because C++ owns allocation, this is where an InterlockedCompareExchange on a
// per-slot reservation word belongs. Left as a follow-up so the port stays a
// faithful 1:1 of the Python find/claim behavior first.
int Manager::FindOrClaimAccountSlot(const std::string& email) {
    int index = FindAccountSlot(email);
    if (index != -1) {
        return index;
    }
    index = FindEmptyOrExpiredSlot(/*allow_expired_reclaim=*/true);
    if (index == -1) {
        return -1;
    }
    AccountStruct& slot = View()->AccountData[index];
    slot = AccountStruct{};  // reset on fresh claim
    slot.SlotNumber = index;
    slot.IsSlotActive = true;
    slot.IsAccount = true;
    WriteWideField(slot.AccountEmail, kMaxEmailLen, email);
    slot.Key.HWND = reinterpret_cast<uint64_t>(::GetActiveWindow());  // parity field; identity is the email
    slot.Key.EntityType = 0;
    slot.Key.LocalIndex = 0;
    return index;
}

// --- payload fill ------------------------------------------------------------

void Manager::ZeroAccountPayload(AccountStruct& slot) {
    // Zero the game-derived payload but KEEP ownership/identity so the slot stays
    // findable-by-email and reads as "owner present, data zeroed" (see plan).
    slot.AgentData = AgentDataStruct{};
    slot.InventoryBags = InventoryBagsStruct{};
    slot.AgentPartyData = AgentPartyStruct{};
    slot.RankData = RankStruct{};
    slot.FactionData = FactionStruct{};
    slot.TitlesData = TitlesStruct{};
    slot.QuestLog = QuestLogStruct{};
    slot.ExperienceData = ExperienceStruct{};
    slot.MissionData = MissionDataStruct{};
    slot.UnlockedSkills = UnlockedSkillsStruct{};
    slot.AvailableCharacters = AvailableCharacterStruct{};
}

namespace {

// Bounded wide-string copy from a possibly-unterminated fixed game buffer.
void WriteWideBounded(wchar_t* dst, size_t dcap, const wchar_t* src, size_t smax) {
    if (!dst || dcap == 0) {
        return;
    }
    std::wmemset(dst, 0, dcap);
    if (!src) {
        return;
    }
    size_t n = 0;
    while (n < smax && n + 1 < dcap && src[n] != 0) {
        dst[n] = src[n];
        ++n;
    }
}

void CopyBitmap(uint32_t* dst, uint32_t cap, const GW::GWArray<uint32_t>& src) {
    const uint32_t n = src.size() < cap ? src.size() : cap;
    // Write every entry to its final value in one pass (no wipe-then-refill).
    for (uint32_t i = 0; i < cap; ++i) {
        dst[i] = (i < n) ? src[i] : 0;
    }
}

// SkillbarStruct.from_context / from_hero_context.
void FillSkillbar(SkillbarStruct& out, const GW::Context::Skillbar* sb) {
    if (!sb) {
        return;
    }
    for (uint32_t i = 0; i < kMaxSkills; ++i) {
        const GW::Context::SkillbarSkill& s = sb->skills[i];
        out.Skills[i].Id = static_cast<uint32_t>(s.skill_id);
        out.Skills[i].Recharge = static_cast<float>(s.GetRecharge());
        out.Skills[i].Adrenaline = static_cast<float>(s.adrenaline_a);
    }
}

// AttributesStruct.from_context: slot the agent's attributes by attribute id.
void FillAttributes(AttributesStruct& out, uint32_t agent_id) {
    const GW::Context::Attribute* attrs = GW::party::get_agent_attributes(agent_id);
    if (!attrs) {
        return;
    }
    for (uint32_t i = 0; i < 54; ++i) {  // PartyAttribute holds a fixed [54] array
        const uint32_t id = static_cast<uint32_t>(attrs[i].id);
        if (id >= kMaxAttributes) {
            continue;
        }
        // Skip empty trailing slots so they can't clobber a real attribute 0
        // (the array is a tagged list, not necessarily id-indexed).
        if (id == 0 && attrs[i].level == 0 && attrs[i].level_base == 0) {
            continue;
        }
        out.Attributes[id].Id = id;
        out.Attributes[id].Value = attrs[i].level;
        out.Attributes[id].BaseValue = attrs[i].level_base;
    }
}

// BuffStruct.from_context: buffs (maintained/upkeep) first, then durational
// effects, matching the Python GetBuffs()+GetEffects() concatenation order.
void FillBuffs(BuffStruct& out, uint32_t agent_id) {
    // Write each of the 240 slots exactly once to its final value: active
    // buffs/effects first, then the now-unused tail cleared. Every field of an
    // active slot is set so a slot reused from a prior frame is fully overwritten.
    uint32_t w = 0;
    if (GW::Context::BuffArray* buffs = GW::effects::GetAgentBuffs(agent_id)) {
        const uint32_t n = buffs->size();
        for (uint32_t i = 0; i < n && w < kMaxBuffs; ++i, ++w) {
            const GW::Context::Buff& b = (*buffs)[i];
            out.Buffs[w].SkillId = static_cast<uint32_t>(b.skill_id);
            out.Buffs[w].Duration = 0.0f;
            out.Buffs[w].Remaining = 0.0f;
            out.Buffs[w].TargetAgentID = b.target_agent_id;
            out.Buffs[w].Type = 1;  // maintained/upkeep buff
        }
    }
    if (GW::Context::EffectArray* effects = GW::effects::GetAgentEffects(agent_id)) {
        const uint32_t n = effects->size();
        for (uint32_t i = 0; i < n && w < kMaxBuffs; ++i, ++w) {
            const GW::Context::Effect& e = (*effects)[i];
            out.Buffs[w].SkillId = static_cast<uint32_t>(e.skill_id);
            out.Buffs[w].Duration = e.duration;
            out.Buffs[w].Remaining = static_cast<float>(e.GetTimeRemaining());
            out.Buffs[w].TargetAgentID = 0;
            out.Buffs[w].Type = 2;  // durational effect
        }
    }
    for (; w < kMaxBuffs; ++w) {
        out.Buffs[w].SkillId = 0;
        out.Buffs[w].Duration = 0.0f;
        out.Buffs[w].Remaining = 0.0f;
        out.Buffs[w].TargetAgentID = 0;
        out.Buffs[w].Type = 0;
    }
}

void FillInventoryBag(InventoryBagStruct& out, const GW::Context::Bag* bag, uint32_t bag_id) {
    // Gather this frame's occupancy, then write all 20 slots once to their final
    // value (empty slots -> 0). No wipe-then-refill.
    uint32_t model[kMaxInventoryBagSlots] = {};
    uint32_t qty[kMaxInventoryBagSlots] = {};
    if (bag) {
        const GW::Context::ItemArray& items = bag->items;
        const uint32_t n = items.size();
        for (uint32_t i = 0; i < n; ++i) {
            const GW::Context::Item* it = items[i];
            if (!it) {
                continue;
            }
            const uint32_t s = it->slot;
            if (s >= kMaxInventoryBagSlots) {
                continue;
            }
            model[s] = it->model_id;
            qty[s] = it->quantity;
        }
    }
    out.BagID = bag_id;
    out.Size = bag ? bag->items_count : 0;
    for (uint32_t s = 0; s < kMaxInventoryBagSlots; ++s) {
        out.Slots[s].BagID = bag_id;
        out.Slots[s].Slot = s;
        out.Slots[s].ModelID = model[s];
        out.Slots[s].Quantity = qty[s];
    }
}

// Backpack/BeltPouch/Bag1/Bag2 == GW bag ids 1..4 (matches the Python Bags enum).
void FillInventory(InventoryBagsStruct& out) {
    const GW::Context::Inventory* inv = GW::Context::GetInventory();
    FillInventoryBag(out.Backpack, inv ? inv->backpack : nullptr, 1);
    FillInventoryBag(out.BeltPouch, inv ? inv->belt_pouch : nullptr, 2);
    FillInventoryBag(out.Bag1, inv ? inv->bag1 : nullptr, 3);
    FillInventoryBag(out.Bag2, inv ? inv->bag2 : nullptr, 4);
}

// AgentPartyStruct.from_context: party id, own party number, ticked, leader.
void FillAgentPartyData(AgentPartyStruct& out, uint32_t self_agent_id) {
    const GW::Context::PartyInfo* party = GW::party::get_party_info(0);
    if (!party) {
        return;  // keep last-good; never wipe mid-operation
    }
    uint32_t own_login = 0;
    if (const GW::Context::Agent* a = GW::agent::GetAgentByID(self_agent_id)) {
        if (const GW::Context::AgentLiving* l = a->GetAsAgentLiving()) {
            own_login = l->login_number;
        }
    }
    uint32_t position = 0;
    bool ticked = false;
    const GW::GWArray<GW::Context::PlayerPartyMember>& players = party->players;
    for (uint32_t i = 0; i < players.size(); ++i) {
        if (players[i].login_number == own_login) {
            position = i;
            ticked = players[i].ticked();
            break;
        }
    }
    // Overwrite every field in place.
    out.PartyID = party->party_id;
    out.PartyPosition = position;
    out.IsTicked = ticked;
    out.IsPartyLeader = GW::party::get_is_leader();
}

}  // namespace

// Port of AgentDataStruct.from_context. Fills every field derivable from the
// agent pointer (works for self / hero / pet). The Skillbar source (player vs
// hero) is supplied by the caller; self-only fields (UUID / Morale / Target /
// Observing) are set by FillAccountPayload.
void Manager::FillAgentData(AgentDataStruct& out, uint32_t agent_id, const GW::Context::Skillbar* skillbar) {
    // No wipe-then-refill: every field below is overwritten in place. On an
    // invalid agent we keep the last-good data rather than blanking it (the slot
    // was already zeroed once, at claim time).
    if (agent_id == 0) {
        return;
    }

    const GW::Context::Agent* agent = GW::agent::GetAgentByID(agent_id);
    if (!agent) {
        return;
    }

    out.AgentID = agent_id;
    out.Pos = Vec3f{agent->x, agent->y, agent->z};
    out.ZPlane = static_cast<int32_t>(agent->plane);
    out.RotationAngle = agent->rotation_angle;
    out.Velocity = Vec2f{agent->velocity.x, agent->velocity.y};
    out.VisualEffectsMask = agent->visual_effects;

    out.Map.MapID = static_cast<uint32_t>(GW::map::GetMapID());
    out.Map.Region = static_cast<int32_t>(GW::map::GetRegion());
    out.Map.District = static_cast<uint32_t>(GW::map::GetDistrict());
    out.Map.Language = static_cast<int32_t>(GW::map::GetLanguage());

    const GW::Context::AgentLiving* living = agent->GetAsAgentLiving();
    if (living) {
        out.LoginNumber = living->login_number;
        out.PlayerNumber = living->player_number;
        out.Level = living->level;
        out.Profession[0] = static_cast<uint32_t>(living->primary);
        out.Profession[1] = static_cast<uint32_t>(living->secondary);
        out.EffectsMask = living->effects;
        out.TypeMap = living->type_map;
        out.ModelState = living->model_state;
        out.DaggerStatus = living->dagger_status;
        out.WeaponType = living->weapon_type;
        out.WeaponAttackSpeed = living->weapon_attack_speed;
        out.AttackSpeedModifier = living->attack_speed_modifier;
        out.WeaponItemType = living->weapon_item_type;
        out.OffhandItemType = living->offhand_item_type;
        out.AnimationSpeed = living->animation_speed;
        out.AnimationCode = living->animation_code;
        out.AnimationID = living->animation_id;
        // Current/Max are exact; Regen mirrors hp_pips. Health.Pips / Energy.Pips
        // are Python-derived (Utils.calculate_*_pips) - TODO for exact parity.
        out.Health.Current = living->hp;
        out.Health.Max = static_cast<float>(living->max_hp);
        out.Health.Regen = living->hp_pips;
        out.Energy.Current = living->energy;
        out.Energy.Max = static_cast<int32_t>(living->max_energy);
        out.Energy.Regen = living->energy_regen;
        out.Skillbar.CastingSkillID = living->skill;
        if (const wchar_t* name = GW::agent::GetPlayerNameByLoginNumber(living->login_number)) {
            WriteWideBounded(out.CharacterName, kMaxCharLen, name, kMaxCharLen);
        }
    }

    FillSkillbar(out.Skillbar, skillbar);
    FillAttributes(out.Attributes, agent_id);
    FillBuffs(out.Buffs, agent_id);
    // out.Overcast: GAP - overcast is a per-skill constant, no per-agent accessor.
}

void Manager::FillAccountPayload(AccountStruct& slot, const std::string& email, int slot_index, bool gates_ok) {
    // Always keep identity + heartbeat, even during a load, so a long map load
    // cannot false-expire the account slot.
    slot.SlotNumber = static_cast<uint32_t>(slot_index);
    slot.IsSlotActive = true;
    slot.IsAccount = true;
    slot.IsHero = false;
    slot.IsPet = false;
    slot.IsNPC = false;
    WriteWideField(slot.AccountEmail, kMaxEmailLen, email);

    if (!gates_ok) {
        // Map gates unmet: zero the payload but keep the slot latched + heartbeat.
        ZeroAccountPayload(slot);
        slot.LastUpdated = NowTick32();
        return;
    }

    if (slot.AccountName[0] == 0) {
        if (const wchar_t* pname = GW::player::GetPlayerName()) {
            WriteWideBounded(slot.AccountName, kMaxCharLen, pname, kMaxCharLen);
        }
    }

    const uint32_t agent_id = GW::player::GetPlayerAgentId(0);
    FillAgentData(slot.AgentData, agent_id, GW::skillbar::GetPlayerSkillbar());
    slot.AgentData.OwnerAgentID = 0;
    slot.AgentData.HeroID = 0;
    slot.AgentData.TargetID = GW::agent::GetTargetId();
    slot.AgentData.ObservingID = GW::agent::GetObservingId();

    const GW::Context::WorldContext* world = GW::Context::GetWorldContext();
    const GW::Context::CharContext* chr = GW::Context::GetCharContext();

    if (chr) {
        for (uint32_t i = 0; i < 4; ++i) {
            slot.AgentData.UUID[i] = chr->player_uuid[i];
        }
    }

    if (world) {
        slot.AgentData.Morale = world->morale;

        if (const GW::Context::AccountInfo* ai = world->accountInfo) {
            slot.RankData.Rank = ai->rank;
            slot.RankData.Rating = ai->rating;
            slot.RankData.QualifierPoints = ai->qualifier_points;
            slot.RankData.Wins = ai->wins;
            slot.RankData.Losses = ai->losses;
            slot.RankData.TournamentRewardPoints = ai->tournament_reward_points;
        }

        slot.FactionData.Kurzick.Current = world->current_kurzick;
        slot.FactionData.Kurzick.TotalEarned = world->total_earned_kurzick;
        slot.FactionData.Kurzick.Max = world->max_kurzick;
        slot.FactionData.Luxon.Current = world->current_luxon;
        slot.FactionData.Luxon.TotalEarned = world->total_earned_luxon;
        slot.FactionData.Luxon.Max = world->max_luxon;
        slot.FactionData.Imperial.Current = world->current_imperial;
        slot.FactionData.Imperial.TotalEarned = world->total_earned_imperial;
        slot.FactionData.Imperial.Max = world->max_imperial;
        slot.FactionData.Balthazar.Current = world->current_balth;
        slot.FactionData.Balthazar.TotalEarned = world->total_earned_balth;
        slot.FactionData.Balthazar.Max = world->max_balth;

        slot.ExperienceData.Level = world->level;
        slot.ExperienceData.Experience = world->experience;
        slot.ExperienceData.CurrentSkillPoints = world->current_skill_points;
        slot.ExperienceData.TotalEarnedSkillPoints = world->total_earned_skill_points;
        // ExperienceData.ProgressPct is Python-derived (Utils.GetExperienceProgression) - TODO.

        CopyBitmap(slot.MissionData.NormalModeCompleted, kMissionBitmapEntries, world->missions_completed);
        CopyBitmap(slot.MissionData.NormalModeBonus, kMissionBitmapEntries, world->missions_bonus);
        CopyBitmap(slot.MissionData.HardModeCompleted, kMissionBitmapEntries, world->missions_completed_hm);
        CopyBitmap(slot.MissionData.HardModeBonus, kMissionBitmapEntries, world->missions_bonus_hm);

        CopyBitmap(slot.UnlockedSkills.Skills, kSkillBitmapEntries, world->unlocked_character_skills);

        slot.QuestLog.ActiveQuestID = static_cast<uint32_t>(world->active_quest_id);
        const uint32_t qn = world->quest_log.size() < kMaxQuests ? world->quest_log.size() : kMaxQuests;
        for (uint32_t i = 0; i < qn; ++i) {
            const GW::Context::Quest& q = world->quest_log[i];
            slot.QuestLog.Quests[i].QuestID = static_cast<uint32_t>(q.quest_id);
            slot.QuestLog.Quests[i].IsCompleted = (q.log_state & 0x2) != 0;
            slot.QuestLog.Quests[i].MarkerX = q.marker.x;
            slot.QuestLog.Quests[i].MarkerY = q.marker.y;
        }
        for (uint32_t i = qn; i < kMaxQuests; ++i) {  // clear only the now-unused tail
            slot.QuestLog.Quests[i].QuestID = 0;
            slot.QuestLog.Quests[i].IsCompleted = false;
            slot.QuestLog.Quests[i].MarkerX = 0.0f;
            slot.QuestLog.Quests[i].MarkerY = 0.0f;
        }

        slot.TitlesData.ActiveTitleID = static_cast<uint32_t>(GW::player::GetActiveTitleId());
        for (uint32_t i = 0; i < kMaxTitles; ++i) {
            slot.TitlesData.Titles[i].TitleID = i;
            slot.TitlesData.Titles[i].CurrentPoints =
                (i < world->titles.size()) ? world->titles[i].current_points : 0;
        }
    }

    if (GW::GWArray<GW::Context::AvailableCharacterInfo>* chars = GW::player::GetAvailableChars()) {
        const uint32_t cn = chars->size() < kMaxAvailableChars ? chars->size() : kMaxAvailableChars;
        for (uint32_t i = 0; i < cn; ++i) {
            const GW::Context::AvailableCharacterInfo& c = (*chars)[i];
            WriteWideBounded(slot.AvailableCharacters.Characters[i].Name, kMaxCharLen, c.player_name, 20);
            slot.AvailableCharacters.Characters[i].Level = c.level();
            slot.AvailableCharacters.Characters[i].IsPvP = c.is_pvp();
            slot.AvailableCharacters.Characters[i].MapID = static_cast<uint32_t>(c.map_id());
            slot.AvailableCharacters.Characters[i].Professions[0] = c.primary();
            slot.AvailableCharacters.Characters[i].Professions[1] = c.secondary();
            slot.AvailableCharacters.Characters[i].CampaignID = c.campaign();
        }
        for (uint32_t i = cn; i < kMaxAvailableChars; ++i) {  // clear only the now-unused tail
            slot.AvailableCharacters.Characters[i] = GW::multibox::AvailableCharacterUnitStruct{};
        }
    }

    FillInventory(slot.InventoryBags);
    FillAgentPartyData(slot.AgentPartyData, agent_id);

    // slot.InAggro stays a HYBRID (party-scan + range + HeroAI settings), so it is
    // left Python-derived and not written here.

    slot.LastUpdated = NowTick32();
}

// --- hero / pet child slots --------------------------------------------------

int Manager::FindChildSlot(const std::string& email, uint32_t entity_type, uint32_t local_id) const {
    AllAccounts* view = View();
    for (uint32_t i = 0; i < kMaxPlayers; ++i) {
        const AccountStruct& slot = view->AccountData[i];
        const bool is_child = (entity_type == 1) ? slot.IsHero : slot.IsPet;
        if (is_child && slot.Key.EntityType == entity_type && slot.Key.LocalIndex == local_id &&
            SlotEmailEquals(slot.AccountEmail, kMaxEmailLen, email)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int Manager::FindOrClaimChildSlot(const std::string& email, uint32_t entity_type, uint32_t local_id) {
    int index = FindChildSlot(email, entity_type, local_id);
    if (index != -1) {
        return index;
    }
    index = FindEmptyOrExpiredSlot(/*allow_expired_reclaim=*/true);
    if (index == -1) {
        return -1;
    }
    AccountStruct& slot = View()->AccountData[index];
    slot = AccountStruct{};
    slot.SlotNumber = index;
    slot.IsSlotActive = true;
    WriteWideField(slot.AccountEmail, kMaxEmailLen, email);
    slot.Key.HWND = reinterpret_cast<uint64_t>(::GetActiveWindow());
    slot.Key.EntityType = entity_type;
    slot.Key.LocalIndex = local_id;
    return index;
}

void Manager::FillHeroPayload(AccountStruct& slot, const std::string& email, int slot_index,
                              const GW::Context::HeroPartyMember& hero, uint32_t hero_index,
                              uint32_t owner_agent_id) {
    slot.SlotNumber = static_cast<uint32_t>(slot_index);
    slot.IsSlotActive = true;
    slot.IsAccount = false;
    slot.IsHero = true;
    slot.IsPet = false;
    slot.IsNPC = false;
    WriteWideField(slot.AccountEmail, kMaxEmailLen, email);
    slot.Key.EntityType = 1;
    slot.Key.LocalIndex = hero.hero_id;

    FillAgentData(slot.AgentData, hero.agent_id, GW::skillbar::GetHeroSkillbar(hero_index));
    slot.AgentData.AgentID = hero.agent_id;
    slot.AgentData.OwnerAgentID = owner_agent_id;
    slot.AgentData.HeroID = hero.hero_id;
    slot.AgentData.TargetID = 0;
    slot.AgentData.LoginNumber = 0;
    slot.AgentData.Morale = 0;  // TODO(parity): hero morale from party morale info

    // Hero name from the party's HeroInfo roster.
    if (const GW::Context::WorldContext* world = GW::Context::GetWorldContext()) {
        const GW::GWArray<GW::Context::HeroInfo>& roster = world->hero_info;
        for (uint32_t i = 0; i < roster.size(); ++i) {
            if (roster[i].hero_id == hero.hero_id) {
                WriteWideBounded(slot.AgentData.CharacterName, kMaxCharLen, roster[i].name, 20);
                break;
            }
        }
    }

    // Heroes carry no account-wide progression (parity with from_hero_context).
    slot.RankData = RankStruct{};
    slot.FactionData = FactionStruct{};
    slot.TitlesData = TitlesStruct{};
    slot.QuestLog = QuestLogStruct{};
    slot.ExperienceData = ExperienceStruct{};
    slot.MissionData = MissionDataStruct{};
    slot.UnlockedSkills = UnlockedSkillsStruct{};
    slot.AvailableCharacters = AvailableCharacterStruct{};
    slot.InventoryBags = InventoryBagsStruct{};

    FillAgentPartyData(slot.AgentPartyData, owner_agent_id);
    slot.AgentPartyData.IsPartyLeader = false;

    slot.LastUpdated = NowTick32();
}

void Manager::FillPetPayload(AccountStruct& slot, const std::string& email, int slot_index,
                             const GW::Context::PetInfo& pet, uint32_t owner_agent_id) {
    slot.SlotNumber = static_cast<uint32_t>(slot_index);
    slot.IsSlotActive = true;
    slot.IsAccount = false;
    slot.IsHero = false;
    slot.IsPet = true;
    slot.IsNPC = false;
    WriteWideField(slot.AccountEmail, kMaxEmailLen, email);
    slot.Key.EntityType = 2;
    slot.Key.LocalIndex = 0;

    FillAgentData(slot.AgentData, pet.agent_id, nullptr);
    slot.AgentData.AgentID = pet.agent_id;
    slot.AgentData.OwnerAgentID = pet.owner_agent_id ? pet.owner_agent_id : owner_agent_id;
    slot.AgentData.HeroID = 0;
    slot.AgentData.Morale = 0;  // pets excluded from party-wide morale
    slot.AgentData.TargetID = pet.locked_target_id;
    slot.AgentData.LoginNumber = 0;
    if (pet.pet_name) {
        WriteWideBounded(slot.AgentData.CharacterName, kMaxCharLen, pet.pet_name, kMaxCharLen);
    }

    slot.AgentData.Skillbar = SkillbarStruct{};
    slot.RankData = RankStruct{};
    slot.FactionData = FactionStruct{};
    slot.TitlesData = TitlesStruct{};
    slot.QuestLog = QuestLogStruct{};
    slot.ExperienceData = ExperienceStruct{};
    slot.MissionData = MissionDataStruct{};
    slot.UnlockedSkills = UnlockedSkillsStruct{};
    slot.AvailableCharacters = AvailableCharacterStruct{};
    slot.InventoryBags = InventoryBagsStruct{};

    FillAgentPartyData(slot.AgentPartyData, owner_agent_id);
    slot.AgentPartyData.IsPartyLeader = false;

    slot.LastUpdated = NowTick32();
}

void Manager::UpdateOwnHeroesAndPets(const std::string& email) {
    AllAccounts* view = View();
    const uint32_t self_agent = GW::player::GetPlayerAgentId(0);
    if (self_agent == 0) {
        return;
    }

    if (const GW::Context::PartyInfo* party = GW::party::get_party_info(0)) {
        const GW::GWArray<GW::Context::HeroPartyMember>& heroes = party->heroes;
        for (uint32_t i = 0; i < heroes.size(); ++i) {
            const GW::Context::HeroPartyMember& hero = heroes[i];
            if (GW::agent::GetAgentIdByLoginNumber(hero.owner_player_id) != self_agent) {
                continue;  // not our hero
            }
            const int idx = FindOrClaimChildSlot(email, 1, hero.hero_id);
            if (idx < 0) {
                continue;
            }
            FillHeroPayload(view->AccountData[idx], email, idx, hero, i, self_agent);
        }
    }

    if (const GW::Context::WorldContext* world = GW::Context::GetWorldContext()) {
        const GW::GWArray<GW::Context::PetInfo>& pets = world->pets;
        for (uint32_t i = 0; i < pets.size(); ++i) {
            const GW::Context::PetInfo& pet = pets[i];
            if (pet.owner_agent_id != self_agent) {
                continue;  // not our pet
            }
            const int idx = FindOrClaimChildSlot(email, 2, pet.agent_id);
            if (idx < 0) {
                continue;
            }
            FillPetPayload(view->AccountData[idx], email, idx, pet, self_agent);
            break;  // one pet per player
        }
    }
}

void Manager::UpdateOwnAccount() {
    const std::string email = PY4GW::System::Instance().GetAccountEmail();
    if (email.empty()) {
        return;
    }

    const int slot_index = FindOrClaimAccountSlot(email);
    if (slot_index == -1) {
        return;  // pool full; nothing we can do this frame
    }

    // Zero the payload ONLY during a genuine map load/transition - that is the
    // single case the reader used to cover by losing its email and reading zeroes.
    // During normal play the slot is always filled.
    //
    // Do NOT gate on get_is_party_loaded() or IsInCinematic here: get_is_party_loaded()
    // returns false whenever ANY party member is not flagged connected (routine in a
    // multibox party - a member zoning or on another map), and zeroing on that makes
    // the payload flicker to zero. The legacy Python writer never zeroed for those
    // conditions; it kept the last-good data. GetIsMapLoaded() (game->map != null)
    // plus InstanceType != Loading is the stable, map-load-only signal.
    const bool gates_ok = GW::map::GetIsMapLoaded() &&
                          GW::map::GetInstanceType() != GW::Constants::InstanceType::Loading;

    FillAccountPayload(View()->AccountData[slot_index], email, slot_index, gates_ok);

    // Heroes/pets are map-instance entities (recycled across map loads). Publish
    // them only when gates are met; otherwise they simply expire and are
    // re-claimed on map-ready.
    if (gates_ok) {
        UpdateOwnHeroesAndPets(email);
    }
}

bool Manager::Update() {
    if (!IsValid()) {
        return false;
    }
    if (!PY4GW::System::Instance().HasAccountEmail()) {
        return false;  // anchor not latched yet
    }

    UpdateOwnAccount();  // also publishes this client's heroes and pets

    return true;
}

Manager& RuntimeManager() {
    return g_runtime_manager;
}

}  // namespace GW::multibox
