#pragma once

#include "base/error_handling.h"

#include "base/CrashHandler.h"
#include "base/hook_types.h"
#include "base/logger.h"
#include "base/patterns.h"
#include "base/scanner.h"
#include "GW/common/constants/constants.h"
#include "GW/common/game_pos.h"
#include "GW/common/gw_array.h"
#include "GW/common/gw_list.h"
#include "GW/render/render.h"
#include "GW/context/agent.h"
#include "GW/context/skill.h"

// Forward declarations for types from headers that include ui.h (circular dependency)
namespace GW::chat {
    enum Channel : int;
    using Color = uint32_t;
}
namespace GW::skillbar {
    struct SkillTemplate;
}

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#include <Windows.h>

namespace GW::ui {

    enum class UIMessage : uint32_t {
        kNone = 0x0,
        kResize = 0x8,
        kInitFrame = 0x9,
        kDestroyFrame = 0xb,
        kKeyDown = 0x20, // wparam = UIPacket::kKeyAction* - Updated from 0x1e to 0x20
        kSetFocus = 0x21, // wparam = 1 or 0
        kKeyUp = 0x22, // wparam = UIPacket::kKeyAction*
        kMouseClick = 0x24, // wparam = UIPacket::kMouseClick*
        kMouseCoordsClick = 0x26, // wparam = UIPacket::kMouseCoordsClick*
        kMouseUp = 0x28, // wparam = UIPacket::kMouseAction*
        kToggleButtonDown = 0x2E,
        kMouseClick2 = 0x31, // wparam = UIPacket::kMouseAction*
        kMouseAction = 0x32, // wparam = UIPacket::kMouseAction*
        kSetLayout = 0x37,
        kMeasureContent = 0x38,
        kRefreshContent = 0x3B,


        // High bit messages start at 0x10000000
        kHighBitBase = 0x10000000,
        kRerenderAgentModel = 0x10000007, // 0x10000007, wparam = uint32_t agent_id
        kAgentDestroy = 0x10000008, // 0x10000008, wparam = uint32_t agent_id
        kUpdateAgentEffects = 0x10000009,
        kAgentSpeechBubble = 0x10000017,
        kShowAgentNameTag = 0x10000019,          // 0x10000019, wparam = AgentNameTagInfo*
        kHideAgentNameTag = 0x1000001A,
        kSetAgentNameTagAttribs = 0x1000001B,    // 0x1000001B, wparam = AgentNameTagInfo*
        kSetAgentProfession = 0x1000001D,        // 0x1000001D, wparam = UIPacket::kSetAgentProfession*
        kChangeTarget = 0x10000020,              // 0x10000020, wparam = UIPacket::kChangeTarget*
        kAgentSkillActivated = 0x10000024, // kAgentSkillPacket
        kAgentSkillActivatedInstantly = 0x10000025, // kAgentSkillPacket
        kAgentSkillCancelled = 0x10000026,       // kAgentSkillPacket
        kAgentStartCasting = 0x10000027,     // 0x10000027, wparam = UIPacket::kAgentStartCasting*
        kShowMapEntryMessage = 0x10000029,       // 0x10000029, wparam = { wchar_t* title, wchar_t* subtitle }
        kSetCurrentPlayerData = 0x1000002A,      // 0x1000002A, fired after setting the worldcontext player name
        kPostProcessingEffect = 0x10000034,      // 0x10000034, Triggered when drunk. wparam = UIPacket::kPostProcessingEffect
        kHeroAgentAdded = 0x10000038,            // 0x10000038, hero assigned to agent/inventory/ai mode
        kHeroDataAdded = 0x10000039,             // 0x10000039, hero info received from server (name, level etc)
        kShowXunlaiChest = 0x10000040,           // 0x10000040
        kMinionCountUpdated = 0x10000046,        // 0x10000046
        kMoraleChange = 0x10000047,              // 0x10000047, wparam = {agent id, morale percent }
        kLoginStateChanged = 0x10000050,         // 0x10000050, wparam = {bool is_logged_in, bool unk }
        kEffectAdd = 0x10000055,                 // 0x10000055, wparam = {agent_id, GW::Effect*}
        kEffectRenew = 0x10000056,               // 0x10000056, wparam = GW::Effect*
        kEffectRemove = 0x10000057,              // 0x10000057, wparam = effect id
        kSkillActivated = 0x1000005b,            // 0x1000005b, wparam ={ uint32_t agent_id , uint32_t skill_id }
        kUpdateSkillbar = 0x1000005E,            // 0x1000005E, wparam ={ uint32_t agent_id , ... }
        kUpdateSkillsAvailable = 0x1000005f,     // 0x1000005f, Triggered on a skill unlock, profession change or map load
        kPlayerTitleChanged = 0x10000064,        // 0x10000064, wparam = { uint32_t player_id, uint32_t title_id }
        kTitleProgressUpdated = 0x10000065,      // 0x10000065, wparam = title_id
        kExperienceGained = 0x10000066,          // 0x10000066, wparam = experience amount
        kWriteToChatLog = 0x1000007F,                // 0x1000007F, wparam = UIPacket::kWriteToChatLog*
        kWriteToChatLogWithSender = 0x10000080,      // 0x10000080, wparam = UIPacket::kWriteToChatLogWithSender*
        kAllyOrGuildMessage = 0x10000081,            // 0x10000081, wparam = UIPacket::kAllyOrGuildMessage*
        kPlayerChatMessage = 0x10000082,             // 0x10000082, wparam = UIPacket::kPlayerChatMessage*
        kFloatingWindowMoved = 0x10000084,           // 0x10000084, wparam = frame_id

        kFriendUpdated = 0x1000008B,                 // 0x1000008B, wparam = { GW::Friend*, ... }
        kMapLoaded = 0x1000008C,                     // 0x1000008C
        kOpenWhisper = 0x10000092,                   // 0x10000092, wparam = wchar* name
        kLoadMapContext = 0x10000098,                // 0x10000098, wparam = UIPacket::kLoadMapContext
        kLogout = 0x1000009D,                        // 0x1000009D, wparam = { bool unknown, bool character_select }
        kCompassDraw = 0x1000009E,                   // 0x1000009E, wparam = UIPacket::kCompassDraw*
        kOnScreenMessage = 0x100000A2,               // 0x100000A2, wparam = wchar_** encoded_string
        kDialogButton = 0x100000A3,                  // 0x100000A3, wparam = DialogButtonInfo*
        kDialogBody = 0x100000A6,                    // 0x100000A6, wparam = DialogBodyInfo*
        kTargetNPCPartyMember = 0x100000B3,          // 0x100000B3, wparam = { uint32_t unk, uint32_t agent_id }
        kTargetPlayerPartyMember = 0x100000B4,       // 0x100000B4, wparam = { uint32_t unk, uint32_t player_number }
        kVendorWindow = 0x100000B5,                  // 0x100000B5, wparam = UIPacket::kVendorWindow
        kVendorItems = 0x100000B9,                   // 0x100000B9, wparam = UIPacket::kVendorItems
        kVendorTransComplete = 0x100000BB,           // 0x100000BB, wparam = *TransactionType
        kVendorQuote = 0x100000BD,                   // 0x100000BD, wparam = UIPacket::kVendorQuote
        kStartMapLoad = 0x100000C2,                  // 0x100000C2, wparam = { uint32_t map_id, ...}
        kWorldMapUpdated = 0x100000C7,               // 0x100000C7, Triggered when an area in the world map has been discovered/updated
        kGuildMemberUpdated = 0x100000DA,            // 0x100000DA, wparam = { GuildPlayer::name_ptr }
        kShowHint = 0x100000E1,                      // 0x100000E1, wparam = { uint32_t icon_type, wchar_t* message_enc }
        kWeaponSetSwapComplete = 0x100000E9,         // 0x100000E9, wparam = UIPacket::kWeaponSwap*
        kWeaponSetSwapCancel = 0x100000EA,           // 0x100000EA

        kWeaponSetUpdated = 0x100000EB,              // 0x100000EB
        kUpdateGoldCharacter = 0x100000EC,           // 0x100000EC, wparam = { uint32_t unk, uint32_t gold_character }
        kUpdateGoldStorage = 0x100000ED,             // 0x100000ED, wparam = { uint32_t unk, uint32_t gold_storage }
        kInventorySlotUpdated = 0x100000EE,          // 0x100000EE, Triggered when an item is moved into a slot
        kEquipmentSlotUpdated = 0x100000EF,          // 0x100000EF, Triggered when an item is moved into a slot
        kInventorySlotCleared = 0x100000F1,          // 0x100000F1, Triggered when an item has been removed from a slot
        kEquipmentSlotCleared = 0x100000F2,          // 0x100000F2, Triggered when an item has been removed from a slot
        kPvPWindowContent = 0x100000FA,              // 0x100000FA
        kPreStartSalvage = 0x10000102,               // 0x10000102, { uint32_t item_id, uint32_t kit_id }
        kTomeSkillSelection = 0x10000103,            // 0x10000103, wparam = UIPacket::kTomeSkillSelection*
        kTradePlayerUpdated = 0x10000105,            // 0x10000105, wparam = GW::TraderPlayer*
        kItemUpdated = 0x10000106,                   // 0x10000106, wparam = UIPacket::kItemUpdated*
        kMapChange = 0x10000111,                     // 0x10000111, wparam = map id
        kCalledTargetChange = 0x10000115,            // 0x10000115, wparam = { player_number, target_id }
        kErrorMessage = 0x10000119,                  // 0x10000119, wparam = { int error_index, wchar_t* error_encoded_string }
        kPartyHardModeChanged = 0x1000011A,          // 0x1000011A, wparam = { int is_hard_mode }
        kPartyAddHenchman = 0x1000011B,              // 0x1000011B
        kPartyRemoveHenchman = 0x1000011C,           // 0x1000011C
        kPartyAddHero = 0x1000011E,                  // 0x1000011E
        kPartyRemoveHero = 0x1000011F,               // 0x1000011F
        kPartyAddPlayer = 0x10000124,                // 0x10000124
        kPartyRemovePlayer = 0x10000126,             // 0x10000126
        kDisableEnterMissionBtn = 0x1000012A,        // 0x1000012A, wparam = boolean (1 = disabled, 0 = enabled)
        kShowCancelEnterMissionBtn = 0x1000012D,     // 0x1000012D
        kPartyDefeated = 0x1000012F,                 // 0x1000012F
        kPartySearchInviteReceived = 0x10000137,     // 0x10000137, wparam = UIPacket::kPartySearchInviteReceived*
        kPartySearchInviteSent = 0x10000139,         // 0x10000139
        kPartyShowConfirmDialog = 0x1000013A,        // 0x1000013A, wparam = UIPacket::kPartyShowConfirmDialog
        kPreferenceEnumChanged = 0x10000140,         // 0x10000140, wparam = UiPacket::kPreferenceEnumChanged
        kPreferenceFlagChanged = 0x10000141,         // 0x10000141, wparam = UiPacket::kPreferenceFlagChanged
        kPreferenceValueChanged = 0x10000142,        // 0x10000142, wparam = UiPacket::kPreferenceValueChanged
        kUIPositionChanged = 0x10000143,             // 0x10000143, wparam = UIPacket::kUIPositionChanged
        kPreBuildLoginScene = 0x10000144,            // 0x10000144, Called with no args right before login scene is drawn

        kQuestAdded = 0x1000014E,                    // 0x1000014E, wparam = { quest_id, ... }
        kQuestDetailsChanged = 0x1000014F,           // 0x1000014F, wparam = { quest_id, ... }
        kQuestRemoved = 0x10000150,                  // 0x10000150, wparam = { quest_id, ... }
        kClientActiveQuestChanged = 0x10000151,      // 0x10000151, wparam = { quest_id, ... }. Triggered when the game requests the current quest to change
        kServerActiveQuestChanged = 0x10000153,      // 0x10000153, wparam = UIPacket::kServerActiveQuestChanged*. Triggered when the server requests the current quest to change
        kUnknownQuestRelated = 0x10000154,           // 0x10000154
        kDungeonComplete = 0x10000156,               // 0x10000156
        kMissionComplete = 0x10000157,               // 0x10000157
        kVanquishComplete = 0x10000159,              // 0x10000159
        kObjectiveAdd = 0x1000015A,                  // 0x1000015A, wparam = UIPacket::kObjectiveAdd*
        kObjectiveComplete = 0x1000015B,             // 0x1000015B, wparam = UIPacket::kObjectiveComplete*
        kObjectiveUpdated = 0x1000015C,              // 0x1000015C, wparam = UIPacket::kObjectiveUpdated*
        kTradeSessionStart = 0x10000165,             // 0x10000165, wparam = { trade_state, player_number }
        kTradeSessionUpdated = 0x1000016b,           // 0x1000016b, no args

        kTriggerLogoutPrompt = 0x1000016E,           // 0x1000016E, no args
        kToggleOptionsWindow = 0x1000016F,           // 0x1000016F, no args
        kRedrawItem = 0x10000174,                    // 0x10000174, wparam = uint32_t item_id
        kCheckUIState = 0x10000176,                  // 0x10000175
        kCloseSettings = 0x10000177,                 // 0x10000176
        kChangeSettingsTab = 0x10000178,             // 0x10000177, wparam = uint32_t is_interface_tab
        kDestroyUIPositionOverlay = 0x1000017D,      // 0x1000017C
        kEnableUIPositionOverlay = 0x1000017E,       // 0x1000017D, wparam = uint32_t enable

        kGuildHall = 0x10000180,                     // 0x1000017F, wparam = gh key (uint32_t[4])
        kLeaveGuildHall = 0x10000182,                // 0x10000181
        kTravel = 0x10000183,                        // 0x10000182
        kOpenWikiUrl = 0x10000184,                   // 0x10000183, wparam = char* url
        kSetPreGameContext_Value0 = 0x10000187,      // 0x10000187, wparam = uint32_t value
        kGetPreGameContext_Value0 = 0x10000189,      // 0x10000189, lparam = *uint32_t value_out
        kSetPreGameContext_Value1 = 0x1000018A,      // 0x1000018A, wparam = uint32_t value
        kGetPreGameContext_Value1 = 0x1000018B,      // 0x1000018B, lparam = *uint32_t value_out
        kAppendMessageToChat = 0x10000194,           // 0x10000194, wparam = wchar_t* message
        kHideHeroPanel = 0x100001A2,                 // 0x100001A2, wparam = hero_id
        kShowHeroPanel = 0x100001A3,                 // 0x100001A3, wparam = hero_id
        kGetInventoryAgentId = 0x100001A7,           // 0x100001A7, wparam = 0, lparam = uint32_t* agent_id_out. Used to fetch which agent is selected
        kInventoryRelated1 = 0x100001A8,             // 0x100001A8
        kInventoryRelated2 = 0x100001A9,             // 0x100001A9
        kInventoryRelated3 = 0x100001AA,             // 0x100001AA
        kEquipItem = 0x100001AB,                     // 0x100001AB, wparam = { item_id, agent_id }
        kMoveItem = 0x100001AC,                      // 0x100001AC, wparam = { item_id, to_bag, to_slot, bool prompt }
        kItemRelated_1 = 0x100001AD,                 // 0x100001AD
        kItemTooltip = 0x100001AE,                   // 0x100001AE
        kItemRelated_3 = 0x100001AF,                 // 0x100001AF
        kItemRelated_4 = 0x100001B0,                 // 0x100001B0
        kInitiateTrade = 0x100001B1,                 // 0x100001B1
        kInventoryAgentChanged = 0x100001C1,         // 0x100001C1, Triggered when inventory needs updating due to agent change; no args
        kInventoryRelated_1 = 0x100001C2,            // 0x100001C2
        kInventoryRelated_2 = 0x100001C3,            // 0x100001C3
        kMissionStatusRelated = 0x100001C4,          // 0x100001C4
        kUnused_1c2 = 0x100001C5,                    // 0x100001C5
        kCollapseExpandSkillListSection = 0x100001C6,// 0x100001C6
        kTemplateRelated_1 = 0x100001C7,             // 0x100001C7
        kTemplateRelated_2 = 0x100001C8,             // 0x100001C8
        kPromptSaveTemplate = 0x100001C9,            // 0x100001C9
        kOpenTemplate = 0x100001CA,                  // 0x100001CA, wparam = GW::UI::ChatTemplate*
        kTemplateRelated_3 = 0x100001CB,             // 0x100001CB
        kTemplateRelated_4 = 0x100001CC,             // 0x100001CC

        // GWCA Client to Server commands. Only added the ones that are used for hooks, everything else goes straight into GW

        kSendEnterMission = 0x30000000 | 0x2,  // wparam = uint32_t arena_id
        kSendLoadSkillbar = 0x30000000 | 0x3,  // wparam = UIPacket::kSendLoadSkillbar*
        kSendPingWeaponSet = 0x30000000 | 0x4,  // wparam = UIPacket::kSendPingWeaponSet*
        kSendMoveItem = 0x30000000 | 0x5,  // wparam = UIPacket::kSendMoveItem*
        kSendMerchantRequestQuote = 0x30000000 | 0x6,  // wparam = UIPacket::kSendMerchantRequestQuote*
        kSendMerchantTransactItem = 0x30000000 | 0x7,  // wparam = UIPacket::kSendMerchantTransactItem*
        kSendUseItem = 0x30000000 | 0x8,  // wparam = UIPacket::kSendUseItem*
        kSendSetActiveQuest = 0x30000000 | 0x9,  // wparam = uint32_t quest_id
        kSendAbandonQuest = 0x30000000 | 0xA, // wparam = uint32_t quest_id
        kSendChangeTarget = 0x30000000 | 0xB, // wparam = UIPacket::kSendChangeTarget* // e.g. tell the gw client to focus on a different target


        kSendMoveToWorldPoint = 0x30000000 | 0xC, // wparam = GW::GamePos* // e.g. Clicking on the ground in the 3d world to move there
        kSendInteractNPC = 0x30000000 | 0xD, // wparam = UIPacket::kInteractAgent*
        kSendInteractGadget = 0x30000000 | 0xE, // wparam = UIPacket::kInteractAgent*
        kSendInteractItem = 0x30000000 | 0xF, // wparam = UIPacket::kInteractAgent*
        kSendInteractEnemy = 0x30000000 | 0x10, // wparam = UIPacket::kInteractAgent*
        kSendInteractPlayer = 0x30000000 | 0x11, // wparam = uint32_t agent_id // NB: calling target is a separate packet
        kSendCallTarget = 0x30000000 | 0x13, // wparam = { uint32_t call_type, uint32_t agent_id } // also used to broadcast morale, death penalty, "I'm following X", etc
        kSendAgentDialog = 0x30000000 | 0x14, // wparam = uint32_t agent_id // e.g. switching tabs on a merchant window, choosing a response to an NPC dialog
        kSendGadgetDialog = 0x30000000 | 0x15, // wparam = uint32_t agent_id // e.g. opening locked chest with a key
        kSendDialog = 0x30000000 | 0x16, // wparam = dialog_id // internal use


        kStartWhisper = 0x30000000 | 0x17, // wparam = UIPacket::kStartWhisper*
        kGetSenderColor = 0x30000000 | 0x18, // wparam = UIPacket::kGetColor* // Get chat sender color depending on channel, output object passed by reference
        kGetMessageColor = 0x30000000 | 0x19, // wparam = UIPacket::kGetColor* // Get chat message color depending on channel, output object passed by reference
        kSendChatMessage = 0x30000000 | 0x1B, // wparam = UIPacket::kSendChatMessage*
        kLogChatMessage = 0x30000000 | 0x1D, // wparam = UIPacket::kLogChatMessage*. Triggered when a message wants to be added to the persistent chat log.
        kRecvWhisper = 0x30000000 | 0x1E, // wparam = UIPacket::kRecvWhisper*
        kPrintChatMessage = 0x30000000 | 0x1F, // wparam = UIPacket::kPrintChatMessage*. Triggered when a message wants to be added to the in-game chat window.
        kSendWorldAction = 0x30000000 | 0x20, // wparam = UIPacket::kSendWorldAction*
        kSetRendererValue = 0x30000000 | 0x21, // wparam = UIPacket::kSetRendererValue
        kIdentifyItem = 0x30000000 | 0x22  // wparam = UIPacket::kIdentifyItem
    };

    enum WindowID : uint32_t {
        WindowID_Dialogue1 = 0x0,
        WindowID_Dialogue2 = 0x1,
        WindowID_MissionGoals = 0x2,
        WindowID_DropBundle = 0x3,
        WindowID_Chat = 0x4,
        WindowID_InGameClock = 0x6,
        WindowID_Compass = 0x7,
        WindowID_DamageMonitor = 0x8,
        WindowID_PerformanceMonitor = 0xB,
        WindowID_EffectsMonitor = 0xC,
        WindowID_Hints = 0xD,
        WindowID_MissionStatusAndScoreDisplay = 0xF,
        WindowID_Notifications = 0x11,
        WindowID_Skillbar = 0x14,
        WindowID_SkillMonitor = 0x15,
        WindowID_UpkeepMonitor = 0x17,
        WindowID_SkillWarmup = 0x18,
        WindowID_Menu = 0x1A,
        WindowID_EnergyBar = 0x1C,
        WindowID_ExperienceBar = 0x1D,
        WindowID_HealthBar = 0x1E,
        WindowID_TargetDisplay = 0x1F,
        WindowID_MissionProgress = 0xE,
        WindowID_TradeButton = 0x21,
        WindowID_WeaponBar = 0x22,
        WindowID_Hero1 = 0x33,
        WindowID_Hero2 = 0x34,
        WindowID_Hero3 = 0x35,
        WindowID_Hero = 0x36,
        WindowID_SkillsAndAttributes = 0x38,
        WindowID_Friends = 0x3A,
        WindowID_Guild = 0x3B,
        WindowID_Help = 0x3D,
        WindowID_Inventory = 0x3E,
        WindowID_VaultBox = 0x3F,
        WindowID_InventoryBags = 0x40,
        WindowID_MissionMap = 0x42,
        WindowID_Observe = 0x44,
        WindowID_Options = 0x45,
        WindowID_PartyWindow = 0x48, // NB: state flag is ignored for party window, but position is still good
        WindowID_PartySearch = 0x49,
        WindowID_QuestLog = 0x4F,
        WindowID_Merchant = 0x5C,
        WindowID_Hero4 = 0x5E,
        WindowID_Hero5 = 0x5F,
        WindowID_Hero6 = 0x60,
        WindowID_Hero7 = 0x61,
        WindowID_Count = 0x66
    };

    enum ControlAction : uint32_t {
        ControlAction_None = 0,
        ControlAction_Screenshot = 0xAE,
        // Panels
        ControlAction_CloseAllPanels = 0x85,
        ControlAction_ToggleInventoryWindow = 0x8B,
        ControlAction_OpenScoreChart = 0xBD,
        ControlAction_OpenTemplateManager = 0xD3,
        ControlAction_OpenSaveEquipmentTemplate = 0xD4,
        ControlAction_OpenSaveSkillTemplate = 0xD5,
        ControlAction_OpenParty = 0xBF,
        ControlAction_OpenGuild = 0xBA,
        ControlAction_OpenFriends = 0xB9,
        ControlAction_ToggleAllBags = 0xB8,
        ControlAction_OpenMissionMap = 0xB6,
        ControlAction_OpenBag2 = 0xB5,
        ControlAction_OpenBag1 = 0xB4,
        ControlAction_OpenBelt = 0xB3,
        ControlAction_OpenBackpack = 0xB2,
        ControlAction_OpenSkillsAndAttributes = 0x8F,
        ControlAction_OpenQuestLog = 0x8E,
        ControlAction_OpenWorldMap = 0x8C,
        ControlAction_OpenHero = 0x8A,

        // Weapon sets
        ControlAction_CycleEquipment = 0x86,
        ControlAction_ActivateWeaponSet1 = 0x81,
        ControlAction_ActivateWeaponSet2,
        ControlAction_ActivateWeaponSet3,
        ControlAction_ActivateWeaponSet4,

        ControlAction_DropItem = 0xCD, // drops bundle item >> flags, ashes, etc

        // Chat
        ControlAction_CharReply = 0xBE,
        ControlAction_OpenChat = 0xA1,
        ControlAction_OpenAlliance = 0x88,

        ControlAction_ReverseCamera = 0x90,
        ControlAction_StrafeLeft = 0x91,
        ControlAction_StrafeRight = 0x92,
        ControlAction_TurnLeft = 0xA2,
        ControlAction_TurnRight = 0xA3,
        ControlAction_MoveBackward = 0xAC,
        ControlAction_MoveForward = 0xAD,
        ControlAction_CancelAction = 0xAF,
        ControlAction_Interact = 0x80,
        ControlAction_ReverseDirection = 0xB1,
        ControlAction_Autorun = 0xB7,
        ControlAction_Follow = 0xCC,

        // Targeting
        ControlAction_TargetPartyMember1 = 0x96,
        ControlAction_TargetPartyMember2,
        ControlAction_TargetPartyMember3,
        ControlAction_TargetPartyMember4,
        ControlAction_TargetPartyMember5,
        ControlAction_TargetPartyMember6,
        ControlAction_TargetPartyMember7,
        ControlAction_TargetPartyMember8,
        ControlAction_TargetPartyMember9 = 0xC6,
        ControlAction_TargetPartyMember10,
        ControlAction_TargetPartyMember11,
        ControlAction_TargetPartyMember12,

        ControlAction_TargetNearestItem = 0xC3,
        ControlAction_TargetNextItem = 0xC4,
        ControlAction_TargetPreviousItem = 0xC5,
        ControlAction_TargetPartyMemberNext = 0xCA,
        ControlAction_TargetPartyMemberPrevious = 0xCB,
        ControlAction_TargetAllyNearest = 0xBC,
        ControlAction_ClearTarget = 0xE3,
        ControlAction_TargetSelf = 0xA0, // also 0x96
        ControlAction_TargetPriorityTarget = 0x9F,
        ControlAction_TargetNearestEnemy = 0x93,
        ControlAction_TargetNextEnemy = 0x95,
        ControlAction_TargetPreviousEnemy = 0x9E,

        ControlAction_ShowOthers = 0x89,
        ControlAction_ShowTargets = 0x94,

        ControlAction_CameraZoomIn = 0xCE,
        ControlAction_CameraZoomOut = 0xCF,

        // Party/Hero commands
        ControlAction_ClearPartyCommands = 0xDB,
        ControlAction_CommandParty = 0xD6,
        ControlAction_CommandHero1,
        ControlAction_CommandHero2,
        ControlAction_CommandHero3,
        ControlAction_CommandHero4 = 0x102,
        ControlAction_CommandHero5,
        ControlAction_CommandHero6,
        ControlAction_CommandHero7,

        ControlAction_OpenHero1PetCommander = 0xE0,
        ControlAction_OpenHero2PetCommander,
        ControlAction_OpenHero3PetCommander,
        ControlAction_OpenHero4PetCommander = 0xFE,
        ControlAction_OpenHero5PetCommander,
        ControlAction_OpenHero6PetCommander,
        ControlAction_OpenHero7PetCommander,
        ControlAction_OpenHeroCommander1 = 0xDC,
        ControlAction_OpenHeroCommander2,
        ControlAction_OpenHeroCommander3,
        ControlAction_OpenHeroCommander4 = 0x126,
        ControlAction_OpenHeroCommander5,
        ControlAction_OpenHeroCommander6,
        ControlAction_OpenHeroCommander7,

        ControlAction_Hero1Skill1 = 0xE5,
        ControlAction_Hero1Skill2,
        ControlAction_Hero1Skill3,
        ControlAction_Hero1Skill4,
        ControlAction_Hero1Skill5,
        ControlAction_Hero1Skill6,
        ControlAction_Hero1Skill7,
        ControlAction_Hero1Skill8,
        ControlAction_Hero2Skill1,
        ControlAction_Hero2Skill2,
        ControlAction_Hero2Skill3,
        ControlAction_Hero2Skill4,
        ControlAction_Hero2Skill5,
        ControlAction_Hero2Skill6,
        ControlAction_Hero2Skill7,
        ControlAction_Hero2Skill8,
        ControlAction_Hero3Skill1,
        ControlAction_Hero3Skill2,
        ControlAction_Hero3Skill3,
        ControlAction_Hero3Skill4,
        ControlAction_Hero3Skill5,
        ControlAction_Hero3Skill6,
        ControlAction_Hero3Skill7,
        ControlAction_Hero3Skill8,
        ControlAction_Hero4Skill1 = 0x106,
        ControlAction_Hero4Skill2,
        ControlAction_Hero4Skill3,
        ControlAction_Hero4Skill4,
        ControlAction_Hero4Skill5,
        ControlAction_Hero4Skill6,
        ControlAction_Hero4Skill7,
        ControlAction_Hero4Skill8,
        ControlAction_Hero5Skill1,
        ControlAction_Hero5Skill2,
        ControlAction_Hero5Skill3,
        ControlAction_Hero5Skill4,
        ControlAction_Hero5Skill5,
        ControlAction_Hero5Skill6,
        ControlAction_Hero5Skill7,
        ControlAction_Hero5Skill8,
        ControlAction_Hero6Skill1,
        ControlAction_Hero6Skill2,
        ControlAction_Hero6Skill3,
        ControlAction_Hero6Skill4,
        ControlAction_Hero6Skill5,
        ControlAction_Hero6Skill6,
        ControlAction_Hero6Skill7,
        ControlAction_Hero6Skill8,
        ControlAction_Hero7Skill1,
        ControlAction_Hero7Skill2,
        ControlAction_Hero7Skill3,
        ControlAction_Hero7Skill4,
        ControlAction_Hero7Skill5,
        ControlAction_Hero7Skill6,
        ControlAction_Hero7Skill7,
        ControlAction_Hero7Skill8,

        // Skills
        ControlAction_UseSkill1 = 0xA4,
        ControlAction_UseSkill2,
        ControlAction_UseSkill3,
        ControlAction_UseSkill4,
        ControlAction_UseSkill5,
        ControlAction_UseSkill6,
        ControlAction_UseSkill7,
        ControlAction_UseSkill8

    };

enum class NumberCommandLineParameter : uint32_t {
    Unk1,
    Unk2,
    Unk3,
    FPS,
    Count
};

enum class EnumPreference : uint32_t {
    CharSortOrder,
    AntiAliasing,
    Reflections,
    ShaderQuality,
    ShadowQuality,
    TerrainQuality,
    InterfaceSize,
    FrameLimiter,
    Count = 0x8
};

enum class StringPreference : uint32_t {
    Unk1,
    Unk2,
    LastCharacterName,
    Count = 0x3
};

enum class NumberPreference : uint32_t {
    // number preferences
    AutoTournPartySort,
    ChatState, // 1 == showing chat window, 0 = not showing chat window
    ChatTab,
    DistrictLastVisitedLanguage,
    DistrictLastVisitedLanguage2,
    DistrictLastVisitedNonInternationalLanguage,
    DistrictLastVisitedNonInternationalLanguage2,
    DamageTextSize, // 0 to 100
    FullscreenGamma, // 0 to 100
    InventoryBag, // Selected bag in inventory window
    TextLanguage,
    AudioLanguage,
    ChatFilterLevel,
    RefreshRate,
    ScreenSizeX,
    ScreenSizeY,
    SkillListFilterRarity,
    SkillListSortMethod,
    SkillListViewMode,
    SoundQuality, // 0 to 100
    StorageBagPage,
    Territory,
    TextureQuality, // TextureLod
    UseBestTextureFiltering,
    EffectsVolume, // 0 to 100
    DialogVolume, // 0 to 100
    BackgroundVolume, // 0 to 100
    MusicVolume, // 0 to 100
    UIVolume, // 0 to 100
    Vote,
    WindowPosX,
    WindowPosY,
    WindowSizeX,
    WindowSizeY,
    SealedSeed, // Used in codex arena
    SealedCount, // Used in codex arena
    FieldOfView, // 0 to 100
    CameraRotationSpeed, // 0 to 100
    ScreenBorderless, // 0x1 = Windowed Borderless, 0x2 = Windowed Fullscreen
    MasterVolume, // 0 to 100
    ClockMode,
    MobileUiScale,
    GamepadCursorSpeed,
    LastLoginMethod,
    Count = 44
};

enum class FlagPreference : uint32_t {
    // boolean preferences
    ChannelAlliance = 0x4,
    ChannelEmotes = 0x6,
    ChannelGuild,
    ChannelLocal,
    ChannelGroup,
    ChannelTrade,
    ShowTextInSkillFloaters = 0x11,
    ShowKRGBRatingsInGame,
    AutoHideUIOnLoginScreen = 0x14,
    DoubleClickToInteract,
    InvertMouseControlOfCamera,
    DisableMouseWalking,
    AutoCameraInObserveMode,
    AutoHideUIInObserveMode,
    RememberAccountName = 0x2D,
    IsWindowed,
    ShowSpendAttributesButton = 0x31, // The game uses this hacky method of showing the "spend attributes" button next to the exp bar.
    ConciseSkillDescriptions,
    DoNotShowSkillTipsOnEffectMonitor,
    DoNotShowSkillTipsOnSkillBars,
    MuteWhenGuildWarsIsInBackground = 0x37,
    AutoTargetFoes = 0x39,
    AutoTargetNPCs,
    AlwaysShowNearbyNamesPvP,
    FadeDistantNameTags,
    DoNotCloseWindowsOnEscape = 0x45,
    ShowMinimapOnWorldMap,
    WaitForVSync = 0x54,
    WhispersFromFriendsEtcOnly,
    ShowChatTimestamps,
    ShowCollapsedBags,
    ItemRarityBorder,
    AlwaysShowAllyNames,
    AlwaysShowFoeNames,
    LockCompassRotation = 0x5c,
    EnableGamepad = 0x5d,
    Count = 0x5e
};

struct CompassPoint {
    CompassPoint() : x(0), y(0) {}
    CompassPoint(int _x, int _y) : x(_x), y(_y) {}
    int x;
    int y;
};

typedef void(__cdecl* DecodeStr_Callback)(void* param, const wchar_t* s);

struct ChatTemplate {
    uint32_t        agent_id;
    uint32_t        type; // 0 = build, 1 = equipment
    GW::GWArray<wchar_t>  code;
    wchar_t*        name;
};

struct UIChatMessage {
    uint32_t channel;
    wchar_t* message;
    uint32_t channel2;
};

struct InteractionMessage {
    uint32_t frame_id;
    UIMessage message_id; // Same as UIMessage from UIMgr, but includes things like mouse move, click etc
    void** wParam;
};

typedef void(__cdecl* UIInteractionCallback)(InteractionMessage* message, void* wParam, void* lParam);
typedef void(__fastcall* UICtlCallback)(void* frame_context, void* wParam, void* lParam);

struct Frame;

struct FrameInteractionCallback {
    UIInteractionCallback callback;
    void* uictl_context;
    uint32_t h0008;
};

struct FrameRelation {
    FrameRelation* parent;
    uint32_t field67_0x124;
    uint32_t field68_0x128;
    uint32_t frame_hash_id;
    GW::GwList<FrameRelation> siblings;

    Frame* GetFrame();
    Frame* GetParent() const;
};
static_assert(sizeof(FrameRelation) == 0x1C, "FrameRelation size mismatch");

struct FramePosition {
    uint32_t flags;
    float left;
    float bottom;
    float right;
    float top;

    float content_left;
    float content_bottom;
    float content_right;
    float content_top;

    float unk;
    float scale_factor; // Depends on UI scale
    float viewport_width; // Width in px of available screen height; this may sometimes be scaled down, too!
    float viewport_height; // Height in px of available screen height; this may sometimes be scaled down, too!

    float screen_left;
    float screen_bottom;
    float screen_right;
    float screen_top;

    [[nodiscard]] GW::Vec2f GetTopLeftOnScreen(const Frame* frame = nullptr) const;
    [[nodiscard]] GW::Vec2f GetBottomRightOnScreen(const Frame* frame = nullptr) const;
    [[nodiscard]] GW::Vec2f GetContentTopLeft(const Frame* frame = nullptr) const;
    [[nodiscard]] GW::Vec2f GetContentBottomRight(const Frame* frame = nullptr) const;
    [[nodiscard]] GW::Vec2f GetSizeOnScreen(const Frame* frame = nullptr) const;
    [[nodiscard]] GW::Vec2f GetViewportScale(const Frame* frame = nullptr) const;
};
static_assert(sizeof(FramePosition) == 0x44, "FramePosition size mismatch");

struct TooltipInfo {
    uint32_t bit_field;
    UIInteractionCallback* render;
    uint32_t* payload;
    uint32_t payload_len;
    uint32_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t unk4;
};

struct AgentNameTagInfo {
    /* +h0000 */ uint32_t agent_id;
    /* +h0004 */ uint32_t h0002;
    /* +h0008 */ uint32_t h0003;
    /* +h000C */ wchar_t* name_enc;
    /* +h0010 */ uint8_t h0010;
    /* +h0011 */ uint8_t h0012;
    /* +h0012 */ uint8_t h0013;
    /* +h0013 */ uint8_t background_alpha; // ARGB, NB: Actual color is ignored, only alpha is used
    /* +h0014 */ uint32_t text_color; // ARGB
    /* +h0014 */ uint32_t label_attributes; // bold/size etc
    /* +h001C */ uint8_t font_style; // Text style (bitmask) / bold | 0x1 / strikthrough | 0x80
    /* +h001D */ uint8_t underline; // Text underline (bool) = 0x01 - 0xFF
    /* +h001E */ uint8_t h001E;
    /* +h001F */ uint8_t h001F;
    /* +h0020 */ wchar_t* extra_info_enc; // Title etc
};

struct MapEntryMessage {
    wchar_t* title;
    wchar_t* subtitle;
};

struct DialogBodyInfo {
    uint32_t type;
    uint32_t agent_id;
    wchar_t* message_enc;
};

struct DialogButtonInfo {
    uint32_t button_icon;
    wchar_t* message;
    uint32_t dialog_id;
    uint32_t skill_id;
};

struct DecodingString {
    std::wstring encoded;
    std::wstring decoded;
    void* original_callback;
    void* original_param;
    void* ecx;
    void* edx;
};

struct CreateUIComponentPacket {
    uint32_t frame_id;
    uint32_t component_flags;
    uint32_t tab_index;
    UIInteractionCallback event_callback;
    union {
        void* wparam;
        wchar_t* name_enc;
    };
    wchar_t* component_label;
};

struct ChangeTargetUIMsg {
    uint32_t manual_target_id;
    uint32_t h0008;
    uint32_t auto_target_id;
    uint32_t h0010;
    uint32_t current_target_id;
    uint32_t h0018;
};

enum class TooltipType : uint32_t {
    None = 0x0,
    EncString1 = 0x4,
    EncString2 = 0x6,
    Item = 0x8,
    WeaponSet = 0xC,
    Skill = 0x14,
    Attribute = 0x4000
};

struct FloatingWindow {
    void* unk1;
    wchar_t* name;
    uint32_t unk2;
    uint32_t unk3;
    uint32_t save_preference;
    uint32_t unk4;
    uint32_t unk5;
    uint32_t unk6;
    uint32_t window_id;
};
static_assert(sizeof(FloatingWindow) == 0x24, "FloatingWindow size mismatch");

typedef uint32_t(__cdecl* EnumClampValueFn)(uint32_t pref_id, uint32_t original_value);

struct EnumPreferenceInfo {
    wchar_t* name;
    uint32_t options_count;
    uint32_t* options;
    uint32_t unk;
    uint32_t pref_type;
};

struct NumberPreferenceInfo {
    wchar_t* name;
    uint32_t flags; // & 0x1 if we have to clamp the value
    uint32_t h000C;
    uint32_t h0010;
    EnumClampValueFn clamp_proc; // Clamps upper/lower bounds for this value; GW will assert an error if this actually clamped the value
    void* mapping_proc; // Used to update other UI elments when changed
};

struct TypedScrollablePageContext {
    void* field_0;
    void* field_4;
    uint32_t field_8;
};

struct Frame {
    uint32_t field1_0x0;
    uint32_t field2_0x4;
    uint32_t frame_layout;
    uint32_t field3_0xc;
    uint32_t field4_0x10;
    uint32_t field5_0x14;
    uint32_t visibility_flags;
    uint32_t field7_0x1c;
    uint32_t type;
    uint32_t template_type;
    uint32_t field10_0x28;
    uint32_t field11_0x2c;
    uint32_t field12_0x30;
    uint32_t field13_0x34;
    uint32_t field14_0x38;
    uint32_t field15_0x3c;
    uint32_t field16_0x40;
    uint32_t field17_0x44;
    uint32_t field18_0x48;
    uint32_t field19_0x4c;
    uint32_t field20_0x50;
    uint32_t field21_0x54;
    uint32_t field22_0x58;
    uint32_t field23_0x5c;
    uint32_t field24_0x60;
    uint32_t field24a_0x64;
    uint32_t field24b_0x68;
    uint32_t field25_0x6c;
    uint32_t field26_0x70;
    uint32_t field27_0x74;
    uint32_t field28_0x78;
    uint32_t field29_0x7c;
    uint32_t field30_0x80;
    GW::GWArray<void*> field31_0x84;
    uint32_t field32_0x94;
    uint32_t field33_0x98;
    uint32_t field34_0x9c;
    uint32_t field35_0xa0;
    uint32_t field36_0xa4;
    GW::GWArray<UIInteractionCallback> frame_callbacks; // GW::GWArray<FrameInteractionCallback> frame_callbacks;
    uint32_t child_offset_id; // Offset of this child in relation to its parent
    uint32_t frame_id; // Offset in the global frame array
    uint32_t field40_0xc0;
    uint32_t field41_0xc4;
    uint32_t field42_0xc8;
    uint32_t field43_0xcc;
    uint32_t field44_0xd0;
    uint32_t field45_0xd4;
    FramePosition position;
    uint32_t field63_0x11c;
    uint32_t field64_0x120;
    uint32_t field65_0x124;
    FrameRelation relation;
    uint32_t field73_0x144;
    uint32_t field74_0x148;
    uint32_t field75_0x14c;
    uint32_t field76_0x150;
    uint32_t field77_0x154;
    uint32_t field78_0x158;
    uint32_t field79_0x15c;
    uint32_t field80_0x160;
    uint32_t field81_0x164;
    uint32_t field82_0x168;
    uint32_t field83_0x16c;
    uint32_t field84_0x170;
    uint32_t field85_0x174;
    uint32_t field86_0x178;
    uint32_t field87_0x17c;
    uint32_t field88_0x180;
    uint32_t field89_0x184;
    uint32_t field90_0x188;
    uint32_t frame_state;
    uint32_t field92_0x190;
    uint32_t field93_0x194;
    uint32_t field94_0x198;
    uint32_t field95_0x19c;
    uint32_t field96_0x1a0;
    uint32_t field97_0x1a4;
    uint32_t field98_0x1a8;
    TooltipInfo* tooltip_info;
    uint32_t field100_0x1b0;
    uint32_t field101_0x1b4;
    uint32_t field102_0x1b8;
    uint32_t field103_0x1bc;
    uint32_t field104_0x1c0;
    uint32_t field105_0x1c4;

    bool IsCreated() const { return (frame_state & 0x4) != 0; }
    bool IsHidden() const { return (frame_state & 0x200) != 0; }
    bool IsVisible() const { return !IsHidden(); }
    bool IsDisabled() const { return (frame_state & 0x10) != 0; }
};
static_assert(sizeof(Frame) == 0x1C8, "Frame size mismatch");
static_assert(offsetof(Frame, relation) == 0x128, "Frame::relation offset mismatch");

inline GW::Vec2f FramePosition::GetTopLeftOnScreen(const Frame* frame) const
{
    const auto viewport_scale = GetViewportScale(frame);
    const auto height = frame ? frame->position.viewport_height : viewport_height;
    return {
        screen_left * viewport_scale.x,
        (height - screen_top) * viewport_scale.y
    };
}

inline GW::Vec2f FramePosition::GetBottomRightOnScreen(const Frame* frame) const
{
    const auto viewport_scale = GetViewportScale(frame);
    const auto height = frame ? frame->position.viewport_height : viewport_height;
    return {
        screen_right * viewport_scale.x,
        (height - screen_bottom) * viewport_scale.y
    };
}

inline GW::Vec2f FramePosition::GetContentTopLeft(const Frame* frame) const
{
    const auto viewport_scale = GetViewportScale(frame);
    const auto height = frame ? frame->position.viewport_height : viewport_height;
    return {
        content_left * viewport_scale.x,
        (height - content_top) * viewport_scale.y
    };
}

inline GW::Vec2f FramePosition::GetContentBottomRight(const Frame* frame) const
{
    const auto viewport_scale = GetViewportScale(frame);
    const auto height = frame ? frame->position.viewport_height : viewport_height;
    return {
        content_right * viewport_scale.x,
        (height - content_bottom) * viewport_scale.y
    };
}

inline GW::Vec2f FramePosition::GetSizeOnScreen(const Frame* frame) const
{
    const auto viewport_scale = GetViewportScale(frame);
    return {
        (screen_right - screen_left) * viewport_scale.x,
        (screen_top - screen_bottom) * viewport_scale.y,
    };
}

inline GW::Vec2f FramePosition::GetViewportScale(const Frame* frame) const
{
    const auto screen_width = static_cast<float>(GW::render::GetViewportWidth());
    const auto screen_height = static_cast<float>(GW::render::GetViewportHeight());
    return {
        screen_width / (frame ? frame->position.viewport_width : viewport_width),
        screen_height / (frame ? frame->position.viewport_height : viewport_height)
    };
}

struct WindowPosition {
    uint32_t state;
    GW::Vec2f p1;
    GW::Vec2f p2;

    bool visible() const { return (state & 0x1) != 0; }
    float xAxis(float ratio) const { return p1.x + (p2.x - p1.x) * ratio; }
    float yAxis(float ratio) const { return p1.y + (p2.y - p1.y) * ratio; }
    GW::Vec2f left() const { return GW::Vec2f(p1.x, yAxis(0.5f)); }
    GW::Vec2f right() const { return GW::Vec2f(p2.x, yAxis(0.5f)); }
    GW::Vec2f top() const { return GW::Vec2f(xAxis(0.5f), p1.y); }
    GW::Vec2f bottom() const { return GW::Vec2f(xAxis(0.5f), p2.y); }
    float width() const { return p2.x - p1.x; }
    float height() const { return p2.y - p1.y; }
};

enum class FrameChild : uint32_t {
    FirstChild = 0,
    LastChild = 1,
    NextSibling = 2,
    PrevSibling = 3
};

} // namespace GW::ui

namespace GW::ui {

namespace packet {
enum ActionState : uint32_t {
    MouseDown = 0x6,
    MouseUp = 0x7,
    MouseClick = 0x8,
    MouseDoubleClick = 0x9,
    DragRelease = 0xB,
    KeyDown = 0xE
};

struct kSendCallTarget {
    Context::CallTargetType call_type;
    uint32_t agent_id;
};

struct kMouseCoordsClick {
    float offset_x;
    float offset_y;
    uint32_t h0008;
    uint32_t h000c;
    uint32_t* h0010;
    uint32_t h0014;
};

struct kIdentifyItem {
    uint32_t item_id;
    uint32_t kit_id;
};

struct kShowXunlaiChest {
    uint32_t h0000 = 0;
    bool storage_pane_unlocked = true;
    bool anniversary_pane_unlocked = true;
};

struct kMoveItem {
    uint32_t item_id;
    uint32_t to_bag_index;
    uint32_t to_slot;
    uint32_t prompt;
};

struct kResize {
    uint32_t h0000 = 0xffffffff;
    float content_width;
    float content_height;
    float h000c = 0;
    float h0010 = 0;
    float content_width2;
    float content_height2;
    float margin_x;
    float margin_y;
    // ...
};

struct kTomeSkillSelection {
    uint32_t item_id;
    uint32_t h0004;
    uint32_t h0008;
};

struct kMeasureContent {
    float max_width;
    float max_height;
    float* size_output;
    uint32_t flags;
};

struct kSetLayout {
    float field_0x0;
    float field_0x4;
    float field_0x8;
    float field_0xc;
    float available_width;
    float available_height;
};

struct kSetAgentProfession {
    Context::AgentID agent_id;
    uint32_t primary;
    uint32_t secondary;
};

struct kWeaponSwap {
    uint32_t weapon_bar_frame_id;
    uint32_t weapon_set_id;
};

struct kWeaponSetChanged {
    uint32_t h0000;
    uint32_t h0004;
    uint32_t h0008;
    uint32_t h000c;
};

struct kChangeTarget {
    uint32_t evaluated_target_id;
    bool has_evaluated_target_changed;
    uint32_t auto_target_id;
    bool has_auto_target_changed;
    uint32_t manual_target_id;
    bool has_manual_target_changed;
};

struct kSendLoadSkillTemplate {
    uint32_t agent_id;
    GW::skillbar::SkillTemplate* skill_template;
};

struct kSetRendererValue {
    uint32_t renderer_mode; // 0 for window, 2 for full screen
    uint32_t metric_id; // TODO: Enum this!
    uint32_t value;
};

struct kEffectAdd {
    uint32_t agent_id;
    Context::Effect* effect;
};

struct kAgentSpeechBubble {
    uint32_t agent_id;
    wchar_t* message;
    uint32_t h0008;
    uint32_t h000c;
};

struct kAgentStartCasting {
    uint32_t agent_id;
    Constants::SkillID skill_id;
    float duration;
    uint32_t h000c;
};

struct kPreStartSalvage {
    uint32_t item_id;
    uint32_t kit_id;
};

struct kServerActiveQuestChanged {
    Constants::QuestID quest_id;
    GamePos marker;
    uint32_t h0024;
    Constants::MapID map_id;
    uint32_t log_state;
};

struct kPrintChatMessage {
    chat::Channel channel;
    wchar_t* message;
    FILETIME timestamp;
    uint32_t is_reprint;
};

struct kPartyShowConfirmDialog {
    uint32_t ui_message_to_send_to_party_frame;
    uint32_t prompt_identitifier;
    wchar_t* prompt_enc_str;
};

struct kUIPositionChanged {
    uint32_t window_id;
    WindowPosition* position;
};

struct kPreferenceFlagChanged {
    FlagPreference preference_id;
    uint32_t new_value;
};

struct kPreferenceValueChanged {
    NumberPreference preference_id;
    uint32_t new_value;
};

struct kPreferenceEnumChanged {
    EnumPreference preference_id;
    uint32_t enum_index;
};

struct kPartySearchInvite {
    uint32_t source_party_search_id;
    uint32_t dest_party_search_id;
};

struct kPostProcessingEffect {
    uint32_t tint;
    float amount;
};

struct kLogout {
    uint32_t unknown;
    uint32_t character_select;
};
static_assert(sizeof(kLogout) == 0x8);

struct KeyAction {
    ControlAction gw_key;
    uint32_t h0004 = 0x4000;
    uint32_t h0008 = 6;
};

struct kMouseClick {
    uint32_t mouse_button; // 0x0 = left, 0x1 = middle, 0x2 = right
    uint32_t is_doubleclick;
    uint32_t unknown_type_screen_pos;
    uint32_t h000c;
    uint32_t h0010;
};

struct MouseAction {
    uint32_t frame_id;
    uint32_t child_offset_id;
    uint32_t current_state;
    void* wparam = nullptr;
    void* lparam = nullptr;
};

struct kWriteToChatLog {
    chat::Channel channel;
    wchar_t* message;
    chat::Channel channel_dupe;
};

struct kPlayerChatMessage {
    chat::Channel channel;
    wchar_t* message;
    uint32_t player_number;
};

struct kInteractAgent {
    uint32_t agent_id;
    bool call_target;
};

struct kSendChangeTarget {
    uint32_t target_id;
    uint32_t auto_target_id;
};

struct kGetColor {
    chat::Color* color;
    chat::Channel channel;
};

struct kWriteToChatLogWithSender {
    uint32_t channel;
    wchar_t* message;
    wchar_t* sender_enc;
};

struct kSendLoadSkillbar {
    uint32_t agent_id;
    uint32_t* skill_ids;
};

struct kSendPingWeaponSet {
    uint32_t agent_id;
    uint32_t weapon_item_id;
    uint32_t offhand_item_id;
};

struct kSendMoveItem {
    uint32_t item_id;
    uint32_t quantity;
    uint32_t bag_id;
    uint32_t slot;
};

struct kSendUseItem {
    uint32_t item_id;
    uint16_t quantity; // Unused, but would be cool
};

struct kSendChatMessage {
    wchar_t* message;
    uint32_t agent_id;
};

struct kLogChatMessage {
    wchar_t* message;
    chat::Channel channel;
};

struct kRecvWhisper {
    uint32_t transaction_id;
    wchar_t* from;
    wchar_t* message;
};

struct kStartWhisper {
    wchar_t* player_name;
};

struct kCompassDraw {
    uint32_t player_number;
    uint32_t session_id;
    uint32_t number_of_points;
    CompassPoint* points;
};

struct kObjectiveAdd {
    uint32_t objective_id;
    wchar_t* name;
    uint32_t type;
};

struct kObjectiveComplete {
    uint32_t objective_id;
};

struct kObjectiveUpdated {
    uint32_t objective_id;
};

struct kItemUpdated {
    uint32_t item_id;
    uint32_t model_file_id;
    uint32_t type;
    uint32_t unk1;
    uint32_t extra_id; // Dye color
    uint32_t materials;
    uint32_t unk2;
    uint32_t interaction; // Flags
    uint32_t price;
    uint32_t model_id;
    uint32_t quantity;
    wchar_t* enc_name;
    uint32_t mod_struct_size;
    uint32_t* mod_struct;
};

struct kInventorySlotUpdated {
    uint32_t unk;
    uint32_t item_id;
    uint32_t bag_index;
    uint32_t slot_id;
};

struct kSendWorldAction {
    Context::WorldActionId action_id;
    Context::AgentID agent_id;
    bool suppress_call_target; // 1 to block "I'm targetting X", but will also only trigger if the key thing is down
};

struct kAllyOrGuildMessage {
    chat::Channel channel;
    wchar_t* message;
    wchar_t* sender;
    wchar_t* guild_tag;
};
}

using ArrayByte = GW::GWArray<unsigned char>;
using SendUIMessageFn = void(__cdecl*)(UIMessage message_id, void* wparam, void* lparam);
using SendFrameUIMessageFn = void(__fastcall*)(GW::GWArray<UIInteractionCallback>* callbacks, void* edx, UIMessage message_id, void* wparam, void* lparam);
using SendFrameUIMessageByIdFn = void(__cdecl*)(uint32_t frame_id, UIMessage message_id, void* wparam, void* lparam);
using CreateHashFromWcharFn = uint32_t(__cdecl*)(const wchar_t* value, int seed);
using GetChildFrameIdFn = uint32_t(__cdecl*)(uint32_t parent_frame_id, uint32_t child_offset);
using FindRelatedFrameFn = uint32_t(__cdecl*)(uint32_t frame_id, uint32_t relation_kind, uint32_t start_after_id);
using GetRootFrameFn = Frame*(__cdecl*)();
using LoadSettingsFn = void(__cdecl*)(uint32_t size, uint8_t* data);
using SetWindowVisibleFn = void(__cdecl*)(uint32_t window_id, uint32_t is_visible, void* wparam, void* lparam);
using SetWindowPositionFn = void(__cdecl*)(uint32_t window_id, WindowPosition* info, void* wparam, void* lparam);
using ValidateAsyncDecodeStrFn = void(__cdecl*)(const wchar_t* value, DecodeStr_Callback callback, void* wparam);
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

using UIMessageCallback = PY4GW::HookCallback<UIMessage, void*, void*>;
using FrameUIMessageCallback = PY4GW::HookCallback<const Frame*, UIMessage, void*, void*>;
using KeyCallback = PY4GW::HookCallback<uint32_t>;
using CreateUIComponentCallback = std::function<void(CreateUIComponentPacket*)>;

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

bool Initialize();
void Shutdown();

Frame* GetRootFrame();
Frame* GetFrameById(uint32_t frame_id);
Frame* GetParentFrame(Frame* frame);
Frame* GetChildFrame(Frame* parent, uint32_t child_offset);
Frame* GetChildFrame(Frame* parent, std::initializer_list<uint32_t> child_offsets);
Frame* GetFrameByLabel(const wchar_t* frame_label);
uint32_t GetFrameIDByLabel(const wchar_t* frame_label);
uint32_t GetFrameIDByHash(uint32_t hash);
uint32_t GetChildFrameID(uint32_t parent_hash, std::vector<uint32_t> child_offsets);
uint32_t GetHashByLabel(const wchar_t* frame_label);
uint32_t GetHashByLabel(const std::string& label);
Frame* GetRelatedFrame(Frame* frame, FrameChild relation_kind, Frame* start_after = nullptr);
Frame* GetRelatedFrameById(uint32_t frame_id, FrameChild relation_kind, uint32_t start_after_id = 0);
Frame* GetChildFromNameHash(Frame* parent, uint32_t name_hash);
std::vector<uint32_t> GetOverlayFrames();
std::vector<uint32_t> GetPopupFrames();
uint32_t GetFrameLayer(Frame* frame);
bool SetFrameLayer(Frame* frame, uint32_t layer);
bool IsAncestorOf(Frame* frame, Frame* other);
uint32_t GetFrameCode(Frame* frame);
bool GetFrameMinSize(Frame* frame, float* width, float* height);
bool GetFrameClientBorder(Frame* frame, float* left, float* top, float* right, float* bottom);
bool GetFrameClipRect(Frame* frame, float* left, float* top, float* right, float* bottom);
bool GetFramePositionEx(Frame* frame, float* x, float* y, float* w, float* h, uint32_t* flags);
bool GetFrameNativeSize(Frame* frame, float* width, float* height);
void* GetFrameContext(Frame* frame);
uint32_t CreateUIComponent(uint32_t parent_frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, void* wparam, const wchar_t* component_label);
uint32_t CreateUIComponent(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, UIInteractionCallback event_callback, wchar_t* name_enc, wchar_t* component_label);
bool DestroyUIComponent(Frame* frame);
bool AddFrameUIInteractionCallback(Frame* frame, UIInteractionCallback callback, void* wparam);
bool TriggerFrameRedraw(Frame* frame);
bool SetFrameMargins(Frame* frame, uint32_t flags, float size[4], float input_mask[4], uint32_t type);
bool SelectDropdownOption(Frame* frame, uint32_t value);
Frame* CreateButtonFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateButtonFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateCtlButtonFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateCtlButtonFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateFlatButtonWithClick(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* label_text = nullptr, bool enable_click = false);
Frame* CreateFlatButtonWithClick(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* label_text = nullptr, bool enable_click = false);
Frame* CreateTextButtonFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* caption = nullptr, wchar_t* component_label = nullptr);
Frame* CreateTextButtonFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* caption = nullptr, wchar_t* component_label = nullptr);
Frame* CreateCheckboxFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateCheckboxFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateScrollableFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, void* page_context = nullptr, wchar_t* component_label = nullptr);
Frame* CreateScrollableFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, void* page_context = nullptr, wchar_t* component_label = nullptr);
Frame* CreateTextLabelFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateTextLabelFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* name_enc = nullptr, wchar_t* component_label = nullptr);
Frame* CreateDropdownFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateDropdownFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateSliderFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateSliderFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateEditableTextFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateEditableTextFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateProgressBar(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateProgressBar(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateTabsFrame(uint32_t parent_frame_id, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
Frame* CreateTabsFrame(Frame* parent, uint32_t component_flags, uint32_t child_index = 0, wchar_t* component_label = nullptr);
bool ButtonClick(Frame* btn_frame);
bool TestMouseAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam = 0, uint32_t lparam = 0);
bool TestMouseClickAction(uint32_t frame_id, uint32_t current_state, uint32_t wparam = 0, uint32_t lparam = 0);
bool SendFrameUIMessage(Frame* frame, UIMessage message_id, void* wparam, void* lparam = nullptr);
bool SendUIMessage(UIMessage message_id, void* wparam = nullptr, void* lparam = nullptr, bool skip_hooks = false);
bool Keydown(ControlAction key, Frame* target = nullptr);
bool Keyup(ControlAction key, Frame* target = nullptr);
bool Keypress(ControlAction key, Frame* target = nullptr);
GW::Constants::Language GetTextLanguage();
uint32_t GetPreference(EnumPreference pref);
uint32_t GetPreferenceOptions(EnumPreference pref, uint32_t** options_out = nullptr);
uint32_t GetPreference(NumberPreference pref);
bool GetPreference(FlagPreference pref);
wchar_t* GetPreference(StringPreference pref);
bool SetPreference(EnumPreference pref, uint32_t value);
bool SetPreference(NumberPreference pref, uint32_t value);
bool SetPreference(FlagPreference pref, bool value);
bool SetPreference(StringPreference pref, wchar_t* value);
bool GetCommandLinePref(const wchar_t* label, wchar_t** out);
bool GetCommandLinePref(const wchar_t* label, uint32_t* out);
bool SetCommandLinePref(const wchar_t* label, wchar_t* value);
bool SetCommandLinePref(const wchar_t* label, uint32_t value);
void SetOpenLinks(bool toggle);
WindowPosition* GetWindowPosition(WindowID window_id);
bool SetWindowVisible(WindowID window_id, bool is_visible);
bool SetWindowPosition(WindowID window_id, WindowPosition* info);
bool DrawOnCompass(unsigned session_id, unsigned point_count, CompassPoint* points);
void LoadSettings(size_t size, uint8_t* data);
ArrayByte* GetSettings();
bool GetIsUIDrawn();
bool GetIsShiftScreenShot();
bool GetIsWorldMapShowing();
bool SetFrameVisible(Frame* frame, bool flag);
bool SetFrameDisabled(Frame* frame, bool flag);
bool SetFrameOpacity(Frame* frame, float opacity, float fade_time = 0.0f);
bool ShowFrame(Frame* frame, bool show);
const wchar_t* GetFrameTitle(Frame* frame);
uint32_t GetParentFrameId(Frame* frame);
float GetFrameOpacity(Frame* frame);
uint32_t GetFrameUserParam(Frame* frame);
bool GetFrameStateBit(Frame* frame, uint32_t bit);
uint32_t GetFrameLimit();
bool SetFrameLimit(uint32_t value);
std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> GetFrameHierarchy();
std::vector<std::pair<uint32_t, uint32_t>> GetFrameCoordsByHash(uint32_t frame_hash);
std::vector<uint32_t> GetFrameArray();
void AsyncDecodeStr(const wchar_t* enc_str, char* buffer, size_t size);
void AsyncDecodeStr(const wchar_t* enc_str, wchar_t* buffer, size_t size);
void AsyncDecodeStr(const wchar_t* enc_str, DecodeStr_Callback callback, void* callback_param = nullptr, GW::Constants::Language language_id = GW::Constants::Language::Unknown);
void AsyncDecodeStr(const wchar_t* enc_str, std::wstring* out, GW::Constants::Language language_id = GW::Constants::Language::Unknown);
bool IsValidEncStr(const wchar_t* enc_str);
bool UInt32ToEncStr(uint32_t value, wchar_t* buffer, size_t count);
uint32_t EncStrToUInt32(const wchar_t* enc_str);
TooltipInfo* GetCurrentTooltip();

void RegisterUIMessageCallback(PY4GW::HookEntry* entry, UIMessage message_id, const UIMessageCallback& callback, int altitude = -0x8000);
void RemoveUIMessageCallback(PY4GW::HookEntry* entry, UIMessage message_id = UIMessage::kNone);
void RegisterFrameUIMessageCallback(PY4GW::HookEntry* entry, UIMessage message_id, const FrameUIMessageCallback& callback, int altitude = -0x8000);
void RemoveFrameUIMessageCallback(PY4GW::HookEntry* entry);
void RegisterKeydownCallback(PY4GW::HookEntry* entry, const KeyCallback& callback);
void RemoveKeydownCallback(PY4GW::HookEntry* entry);
void RegisterKeyupCallback(PY4GW::HookEntry* entry, const KeyCallback& callback);
void RemoveKeyupCallback(PY4GW::HookEntry* entry);
void RegisterCreateUIComponentCallback(PY4GW::HookEntry* entry, const CreateUIComponentCallback& callback, int altitude = -0x8000);
void RemoveCreateUIComponentCallback(PY4GW::HookEntry* entry);
bool InitializeTypedComponentCallbacks();

struct ButtonFrame : Frame {
    static ButtonFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xFF, const wchar_t* button_label = nullptr, const wchar_t* frame_label = nullptr);
    bool GetLabel(const wchar_t** enc_string);
    bool SetLabel(const wchar_t* enc_string);
    bool Click();
    bool MouseAction(packet::ActionState action);
    bool DoubleClick();
};

struct FrameWithValue {
    FrameWithValue() = default;
    virtual ~FrameWithValue() = default;
    FrameWithValue(const FrameWithValue&) = default;
    FrameWithValue(FrameWithValue&&) = default;
    FrameWithValue& operator=(const FrameWithValue&) = default;
    FrameWithValue& operator=(FrameWithValue&&) = default;

    virtual uint32_t GetValue();
    virtual bool SetValue(uint32_t value);
};

struct CheckboxFrame final : ButtonFrame, FrameWithValue {
    static CheckboxFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x8000, uint32_t child_offset_id = 0xFF, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
    bool IsChecked();
    bool SetChecked(bool checked);

    uint32_t GetValue() override;
    bool SetValue(uint32_t value) override;
};

struct DropdownFrame final : Frame, FrameWithValue {
    static DropdownFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xFF, const wchar_t* frame_label = nullptr);
    std::vector<uint32_t> GetOptions();
    bool SelectOption(uint32_t value);
    bool SelectIndex(uint32_t index);
    bool AddOption(const wchar_t* label_enc_string, uint32_t value);
    bool GetCount(uint32_t* count);
    bool GetOptionValue(uint32_t index, uint32_t* value);
    bool GetOptionIndex(uint32_t value, uint32_t* index);
    bool GetSelectedIndex(uint32_t* index);
    bool HasValueMapping();

    uint32_t GetValue() override;
    bool SetValue(uint32_t value) override;
};

struct SliderFrame final : Frame, FrameWithValue {
    static SliderFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0, uint32_t child_offset_id = 0xFF, const wchar_t* frame_label = nullptr);
    bool GetValue(uint32_t* selected_value);
    bool SetValue(uint32_t value) override;

    uint32_t GetValue() override;
};

struct EditableTextFrame : Frame {
    static EditableTextFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0, uint32_t child_offset_id = 0xFF, const wchar_t* frame_label = nullptr);
    const wchar_t* GetValue();
    bool SetValue(const wchar_t* value);
    bool SetMaxLength(uint32_t max_length);
    bool IsReadOnly();
    bool SetReadOnly(bool readonly);
};

struct ProgressBar final : ButtonFrame, FrameWithValue {
    static ProgressBar* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xFF, const wchar_t* frame_label = nullptr);
    uint32_t GetValue() override;
    bool SetValue(uint32_t value) override;
    bool SetMax(uint32_t value);
    bool SetColorId(uint32_t color_id);

    enum class ProgressBarStyle : uint32_t {
        kPeach,
        kPink,
        kGrey,
        kBlue,
        kGreen,
        kRed,
        kPurple,
        kOlive,
        kUnk
    };

    bool SetStyle(ProgressBarStyle style);
};

struct TextLabelFrame : Frame {
    static TextLabelFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x300, uint32_t child_offset_id = 0xFF, const wchar_t* text_label_enc_string = nullptr, const wchar_t* frame_label = nullptr);
    const wchar_t* GetEncodedLabel();
    const wchar_t* GetDecodedLabel();
    bool SetLabel(const wchar_t* enc_string);
    bool SetFont(uint32_t font_id);
};

struct MultiLineTextLabelFrame final : TextLabelFrame {
    const wchar_t* GetEncodedLabel();
    const wchar_t* GetDecodedLabel();
    bool SetLabel(const wchar_t* enc_string);
};

struct TabsFrame : Frame {
    static TabsFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x40000, uint32_t child_offset_id = 0xFF, const wchar_t* frame_label = nullptr);
    Frame* AddTab(const wchar_t* tab_name_enc, uint32_t flags, uint32_t child_offset_id, UIInteractionCallback callback, void* wparam = nullptr);
    bool DisableTab(uint32_t tab_id);
    bool EnableTab(uint32_t tab_id);
    bool RemoveTab(uint32_t tab_id);
    bool GetCurrentTabIndex(uint32_t* tab_id);
    bool GetTabFrameId(uint32_t tab_id, uint32_t* frame_id);
    bool GetIsTabEnabled(uint32_t tab_id, uint32_t* is_enabled);
    Frame* GetTabByLabel(const wchar_t* label);
    Frame* GetCurrentTab();
    bool ChooseTab(Frame* tab_frame);
    bool ChooseTab(uint32_t tab_index);
    ButtonFrame* GetTabButton(Frame* tab_frame);
};

struct ScrollableFrame : Frame {
    using SortHandlerFn = int(__cdecl*)(uint32_t frame_id_1, uint32_t frame_id_2);

    struct ScrollablePageContext {
        uint32_t flags;
        UIInteractionCallback page_callback;
        uint32_t wparam;
    };

    static ScrollableFrame* Create(uint32_t parent_frame_id, uint32_t flags = 0x20000, uint32_t child_offset_id = 0xFF, ScrollablePageContext* context = nullptr, const wchar_t* frame_label = nullptr);
    bool SetSortHandler(SortHandlerFn sort_handler);
    SortHandlerFn GetSortHandler();
    bool ClearItems();
    bool RemoveItem(uint32_t child_offset_id);
    bool AddItem(uint32_t flags, uint32_t child_offset_id, UIInteractionCallback callback);
    uint32_t GetItemFrameId(uint32_t child_offset_id);
    bool GetSelectedValue(uint32_t* selected_value);
    uint32_t GetFirstChildFrameId(uint32_t* offset_of_child_out = nullptr);
    uint32_t GetNextChildFrameId(uint32_t frame_id, uint32_t* offset_of_child_out = nullptr);
    uint32_t GetLastChildFrameId(uint32_t* offset_of_child_out = nullptr);
    uint32_t GetPrevChildFrameId(uint32_t frame_id, uint32_t* offset_of_child_out = nullptr);
    bool GetItemRect(uint32_t child_offset_id, float rect[4]);
    bool GetCount(uint32_t* size);
    uint32_t GetItems(uint32_t* child_frame_id_buffer = nullptr, uint32_t buffer_len = 0);
    Frame* GetPage();
    Frame* SetPage(ScrollablePageContext* context);
};

// uint32_t GetPreference(...);
// bool SetPreference(...);
// bool GetCommandLinePref(...);  // Implemented below.
// bool SetCommandLinePref(...);  // Implemented below.
// bool Default_UICallback(...);  // Deferred: header-declared in legacy UIMgr.h, but no body recovered from GWCA UIMgr.cpp.
// bool IsInControllerMode();     // Deferred: header-declared in legacy UIMgr.h, but no body recovered from GWCA UIMgr.cpp.
// bool IsInControllerCursorMode();  // Deferred: header-declared in legacy UIMgr.h, but no body recovered from GWCA UIMgr.cpp.
// bool SetOpenLinks(bool toggle);
// Frame* CreateFlatButtonWithClick(...);
// Deferred in the fresh UIMgr pass. The only remaining known gaps are legacy declarations
// with no recovered GWCA UIMgr.cpp body.

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
extern TitleBinarySearchFn g_title_binary_search_func;
extern GetTitleFn g_get_title_func;
extern DrawOnCompassFn g_draw_on_compass_func;
extern CreateUIComponentFn g_create_ui_component_func;
extern CreateUIComponentFn g_create_ui_component_original;
extern DestroyUIComponentFn g_destroy_ui_component_func;
extern FrameNewSubclassFn g_frame_new_subclass_func;
extern SetTooltipFn g_set_tooltip_func;
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
extern EnumPreferenceInfo* g_enum_preference_options_addr;
extern NumberPreferenceInfo* g_number_preference_options_addr;
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
extern GW::GWArray<Frame*>* g_frame_array;
extern uintptr_t g_world_map_state_addr;
extern uintptr_t g_preferences_initialized_addr;
extern uintptr_t g_title_table_addr;
extern uintptr_t g_ui_drawn_addr;
extern uintptr_t g_shift_screen_addr;
extern uintptr_t g_game_settings_addr;
extern TooltipInfo*** g_current_tooltip_ptr;
extern WindowPosition* g_window_positions_array;
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

inline uintptr_t FindPatternAddress(const char* name) {
    const auto* pattern = PY4GW::Patterns::Get(name);
    if (!pattern) {
        Logger::Instance().LogError(std::string("Missing or invalid pattern: ") + name, "ui");
        return 0;
    }
    if (!pattern->assertion_file.empty() || !pattern->assertion_message.empty()) {
        return PY4GW::Scanner::FindAssertion(
            pattern->assertion_file.c_str(),
            pattern->assertion_message.c_str(),
            static_cast<uint32_t>(pattern->line_number),
            pattern->offset);
    }
    return PY4GW::Scanner::Find(
        pattern->pattern.c_str(),
        pattern->mask.c_str(),
        pattern->offset,
        pattern->section);
}

inline bool WarnIfMissingAddress(const char* name, uintptr_t address) {
    if (address) {
        return true;
    }
    Logger::Instance().LogWarning(std::string(name) + " is null.", "ui");
    return false;
}

inline bool ResolveFrameArray() {
    CrashContextScope context("startup", "ui", "resolve_frame_array");
    const uintptr_t address = FindPatternAddress("ui.frame_array_anchor");
    if (!Logger::AssertAddress("s_FrameArray_Ref", address, "ui")) {
        return false;
    }
    g_frame_array = *reinterpret_cast<GW::GWArray<Frame*>**>(address);
    return Logger::AssertAddress("s_FrameArray", reinterpret_cast<uintptr_t>(g_frame_array), "ui");
}

inline bool ResolveWorldMapState() {
    CrashContextScope context("startup", "ui", "resolve_world_map_state");
    const uintptr_t address = FindPatternAddress("ui.world_map_state");
    if (!Logger::AssertAddress("WorldMapState_Ref", address, "ui")) {
        return false;
    }
    const uintptr_t candidate = *reinterpret_cast<uintptr_t*>(address);
    if (!Logger::AssertAddress("WorldMapState_Addr", candidate, "ui")) {
        return false;
    }
    g_world_map_state_addr = candidate;
    return true;
}

inline bool ResolveSendFrameUiMessage() {
    CrashContextScope context("startup", "ui", "resolve_send_frame_ui_message");
    const uintptr_t address = FindPatternAddress("ui.send_frame_ui_message_by_id");
    if (!Logger::AssertAddress("SendFrameUIMessageById_Func", address, "ui")) {
        return false;
    }
    g_send_frame_ui_message_by_id_func = reinterpret_cast<SendFrameUIMessageByIdFn>(address);
    g_send_frame_ui_message_func = reinterpret_cast<SendFrameUIMessageFn>(
        PY4GW::Scanner::FunctionFromNearCall(address + 0x67));
    return Logger::AssertAddress("SendFrameUIMessage_Func", reinterpret_cast<uintptr_t>(g_send_frame_ui_message_func), "ui");
}

inline bool ResolveCreateHashFromWchar() {
    CrashContextScope context("startup", "ui", "resolve_create_hash_from_wchar");
    const uintptr_t address = FindPatternAddress("ui.create_hash_from_wchar");
    if (!Logger::AssertAddress("CreateHashFromWchar_Callsite", address, "ui")) {
        return false;
    }
    g_create_hash_from_wchar_func = reinterpret_cast<CreateHashFromWcharFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("CreateHashFromWchar_Func", reinterpret_cast<uintptr_t>(g_create_hash_from_wchar_func), "ui");
}

inline bool ResolveGetChildFrameId() {
    CrashContextScope context("startup", "ui", "resolve_get_child_frame_id");
    const uintptr_t address = FindPatternAddress("ui.get_child_frame_id_anchor");
    if (!Logger::AssertAddress("GetChildFrameId_Callsite", address, "ui")) {
        return false;
    }
    g_get_child_frame_id_func = reinterpret_cast<GetChildFrameIdFn>(
        PY4GW::Scanner::FunctionFromNearCall(address));
    return Logger::AssertAddress("GetChildFrameId_Func", reinterpret_cast<uintptr_t>(g_get_child_frame_id_func), "ui");
}

inline bool ResolveFindRelatedFrame() {
    CrashContextScope context("startup", "ui", "resolve_find_related_frame");
    const uintptr_t address = FindPatternAddress("ui.find_related_frame");
    if (!Logger::AssertAddress("FindRelatedFrame_Func", address, "ui")) {
        return false;
    }
    g_find_related_frame_func = reinterpret_cast<FindRelatedFrameFn>(address);
    return true;
}

inline bool ResolveGetRootFrame() {
    CrashContextScope context("startup", "ui", "resolve_get_root_frame");
    const uintptr_t address = FindPatternAddress("ui.get_root_frame");
    g_get_root_frame_func = reinterpret_cast<GetRootFrameFn>(address);
    return Logger::AssertAddress("GetRootFrame_Func", reinterpret_cast<uintptr_t>(g_get_root_frame_func), "ui");
}

inline bool ResolveSendUiMessage() {
    CrashContextScope context("startup", "ui", "resolve_send_ui_message");
    const uintptr_t address = FindPatternAddress("ui.send_ui_message");
    if (!Logger::AssertAddress("SendUIMessage_Scan", address, "ui")) {
        return false;
    }
    g_send_ui_message_func = reinterpret_cast<SendUIMessageFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("SendUIMessage_Func", reinterpret_cast<uintptr_t>(g_send_ui_message_func), "ui");
}

inline bool ResolveLoadSettings() {
    CrashContextScope context("startup", "ui", "resolve_load_settings");
    const uintptr_t address = FindPatternAddress("ui.load_settings");
    if (!Logger::AssertAddress("LoadSettings_Callsite", address, "ui")) {
        return false;
    }
    g_load_settings_func = reinterpret_cast<LoadSettingsFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("LoadSettings_Func", reinterpret_cast<uintptr_t>(g_load_settings_func), "ui");
}

inline bool ResolveUiDrawn() {
    CrashContextScope context("startup", "ui", "resolve_ui_drawn");
    const uintptr_t address = FindPatternAddress("ui.ui_drawn_anchor");
    if (!Logger::AssertAddress("ui_drawn_ref", address, "ui")) {
        return false;
    }
    g_ui_drawn_addr = *reinterpret_cast<uintptr_t*>(address) - 0x10;
    return Logger::AssertAddress("ui_drawn_addr", g_ui_drawn_addr, "ui");
}

inline bool ResolveShiftScreenshot() {
    CrashContextScope context("startup", "ui", "resolve_shift_screenshot");
    const uintptr_t address = FindPatternAddress("ui.shift_screenshot");
    if (!Logger::AssertAddress("shift_screen_ref", address, "ui")) {
        return false;
    }
    const uintptr_t candidate = *reinterpret_cast<uintptr_t*>(address);
    if (!Logger::AssertAddress("shift_screen_addr", candidate, "ui")) {
        return false;
    }
    g_shift_screen_addr = candidate;
    return true;
}

inline bool ResolveSetTooltip() {
    CrashContextScope context("startup", "ui", "resolve_set_tooltip");
    const uintptr_t address = FindPatternAddress("ui.set_tooltip");
    if (!Logger::AssertAddress("SetTooltip_Func", address, "ui")) {
        return false;
    }
    g_set_tooltip_func = reinterpret_cast<SetTooltipFn>(PY4GW::Scanner::ToFunctionStart(address));
    const uintptr_t ptr_ref = reinterpret_cast<uintptr_t>(g_set_tooltip_func) + 0x9;
    if (!Logger::AssertAddress("CurrentTooltipPtr_Ref", ptr_ref, "ui")) {
        return false;
    }
    g_current_tooltip_ptr = reinterpret_cast<TooltipInfo***>(*reinterpret_cast<uintptr_t*>(ptr_ref));
    const bool func_ok = Logger::AssertAddress("SetTooltip_Func", reinterpret_cast<uintptr_t>(g_set_tooltip_func), "ui");
    const bool ptr_ok = Logger::AssertAddress("CurrentTooltipPtr", reinterpret_cast<uintptr_t>(g_current_tooltip_ptr), "ui");
    return func_ok && ptr_ok;
}

inline bool ResolveGameSettings() {
    CrashContextScope context("startup", "ui", "resolve_game_settings");
    const uintptr_t address = FindPatternAddress("ui.game_settings_addr");
    if (!Logger::AssertAddress("GameSettings_Ref", address, "ui")) {
        return false;
    }
    g_game_settings_addr = *reinterpret_cast<uintptr_t*>(address);
    return Logger::AssertAddress("GameSettings_Addr", g_game_settings_addr, "ui");
}

inline bool ResolveWindowHelpers() {
    CrashContextScope context("startup", "ui", "resolve_window_helpers");
    const uintptr_t address = FindPatternAddress("ui.set_window_visible");
    if (!Logger::AssertAddress("SetWindowVisible_Func", address, "ui")) {
        return false;
    }
    const uintptr_t func = PY4GW::Scanner::ToFunctionStart(address);
    g_set_window_visible_func = reinterpret_cast<SetWindowVisibleFn>(func);
    g_set_window_position_func = reinterpret_cast<SetWindowPositionFn>(func - 0xE0);
    const uintptr_t array_ref = func + 0x49;
    if (!Logger::AssertAddress("window_positions_ref", array_ref, "ui")) {
        return false;
    }
    g_window_positions_array = *reinterpret_cast<WindowPosition**>(array_ref);
    const bool visible_ok = Logger::AssertAddress("SetWindowPosition_Func", reinterpret_cast<uintptr_t>(g_set_window_position_func), "ui");
    const bool array_ok = Logger::AssertAddress("window_positions_array", reinterpret_cast<uintptr_t>(g_window_positions_array), "ui");
    return visible_ok && array_ok;
}

inline bool ResolveValidateAsyncDecode() {
    CrashContextScope context("startup", "ui", "resolve_async_decode");
    const uintptr_t address = FindPatternAddress("ui.validate_async_decode");
    g_validate_async_decode_str_func = reinterpret_cast<ValidateAsyncDecodeStrFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("ValidateAsyncDecodeStr", reinterpret_cast<uintptr_t>(g_validate_async_decode_str_func), "ui");
}

inline bool ResolveTitleHelpers() {
    CrashContextScope context("startup", "ui", "resolve_title_helpers");

    uintptr_t get_title_addr = PY4GW::Scanner::FindAssertion(
        "FrNonclient.cpp",
        "ptr->title.Count()",
        0,
        -0x26);
    if (get_title_addr) {
        get_title_addr = PY4GW::Scanner::ToFunctionStart(get_title_addr, 0xFF);
    }
    if (!Logger::AssertAddress("GetTitle_Func", get_title_addr, "ui")) {
        return false;
    }
    g_get_title_func = reinterpret_cast<GetTitleFn>(get_title_addr);

    for (uintptr_t scan = get_title_addr; scan < get_title_addr + 0x100; ++scan) {
        if (*reinterpret_cast<const uint8_t*>(scan) != 0xB9) {
            continue;
        }

        g_title_table_addr = *reinterpret_cast<const uintptr_t*>(scan + 1);
        if (!(g_title_table_addr &&
            PY4GW::Scanner::IsValidPtr(g_title_table_addr, PY4GW::ScannerSection::Data))) {
            g_title_table_addr = 0;
            break;
        }

        for (uintptr_t callsite = scan + 5; callsite < get_title_addr + 0x100; ++callsite) {
            if (*reinterpret_cast<const uint8_t*>(callsite) != 0xE8) {
                continue;
            }

            const uintptr_t candidate = PY4GW::Scanner::FunctionFromNearCall(callsite, true);
            if (candidate) {
                g_title_binary_search_func = reinterpret_cast<TitleBinarySearchFn>(candidate);
                break;
            }
        }
        break;
    }

    const bool title_table_ok = Logger::AssertAddress("TitleTable_Addr", g_title_table_addr, "ui");
    const bool title_search_ok = Logger::AssertAddress("TitleBinarySearch_Func", reinterpret_cast<uintptr_t>(g_title_binary_search_func), "ui");
    return title_table_ok && title_search_ok;
}

inline bool ResolveDrawOnCompass() {
    CrashContextScope context("startup", "ui", "resolve_draw_on_compass");
    const uintptr_t address = FindPatternAddress("ui.draw_on_compass");
    g_draw_on_compass_func = reinterpret_cast<DrawOnCompassFn>(PY4GW::Scanner::ToFunctionStart(address));
    return Logger::AssertAddress("DrawOnCompass_Func", reinterpret_cast<uintptr_t>(g_draw_on_compass_func), "ui");
}

inline bool ResolveCreateUiComponent() {
    CrashContextScope context("startup", "ui", "resolve_create_ui_component");
    const uintptr_t create_address = FindPatternAddress("ui.create_ui_component");
    if (!Logger::AssertAddress("CreateUIComponent_Scan", create_address, "ui")) {
        return false;
    }
    g_create_ui_component_func = reinterpret_cast<CreateUIComponentFn>(PY4GW::Scanner::ToFunctionStart(create_address));
    const uintptr_t destroy_address = FindPatternAddress("ui.destroy_ui_component");
    if (destroy_address) {
        g_destroy_ui_component_func = reinterpret_cast<DestroyUIComponentFn>(PY4GW::Scanner::ToFunctionStart(destroy_address));
        if (!g_destroy_ui_component_func) {
            Logger::Instance().LogWarning("DestroyUIComponent_Func could not be recovered from the resolved scan.", "ui");
        }
    } else {
        Logger::Instance().LogWarning("DestroyUIComponent_Scan could not be resolved; continuing without destroy support.", "ui");
    }
    return Logger::AssertAddress("CreateUIComponent_Func", reinterpret_cast<uintptr_t>(g_create_ui_component_func), "ui");
}

inline bool ResolveFrameNewSubclass() {
    CrashContextScope context("startup", "ui", "resolve_frame_new_subclass");

    uintptr_t address = PY4GW::Scanner::FindAssertion(
        "\\Code\\Engine\\Frame\\FrApi.cpp",
        "frameId",
        0x467,
        0);
    if (address) {
        g_frame_new_subclass_func = reinterpret_cast<FrameNewSubclassFn>(PY4GW::Scanner::ToFunctionStart(address, 0x100));
    }
    if (!g_frame_new_subclass_func) {
        address = PY4GW::Scanner::Find(
            "\x8D\xB8\xA8\x00\x00\x00\x8B\xCF",
            "xxxxxxxx",
            -0x2D);
        if (address) {
            g_frame_new_subclass_func = reinterpret_cast<FrameNewSubclassFn>(address);
        }
    }

    return Logger::AssertAddress("FrameNewSubclass_Func", reinterpret_cast<uintptr_t>(g_frame_new_subclass_func), "ui");
}

inline bool ResolveTypedComponentPassthrough() {
    CrashContextScope context("startup", "ui", "resolve_typed_component_passthrough");
    const uintptr_t address = FindPatternAddress("ui.typed_component_passthrough");
    if (!Logger::AssertAddress("TypedComponentPassthrough_Scan", address, "ui")) {
        return false;
    }
    g_typed_component_passthrough_func = reinterpret_cast<TypedComponentPassthroughFn>(PY4GW::Scanner::ToFunctionStart(address, 0xFFF));
    return Logger::AssertAddress("TypedComponentPassthrough_Func", reinterpret_cast<uintptr_t>(g_typed_component_passthrough_func), "ui");
}

inline bool ResolvePreferenceReaders() {
    CrashContextScope context("startup", "ui", "resolve_preference_readers");
    const uintptr_t pref_init = FindPatternAddress("ui.preferences_initialized");
    if (pref_init) {
        g_preferences_initialized_addr = *reinterpret_cast<const uintptr_t*>(pref_init);
        WarnIfMissingAddress("PreferencesInitialised_Addr", g_preferences_initialized_addr);
    } else {
        Logger::Instance().LogWarning("PreferencesInitialised_Ref is null.", "ui");
    }

    g_get_string_preference_func = reinterpret_cast<GetStringPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_STRINGS", 0)));
    g_get_flag_preference_func = reinterpret_cast<GetFlagPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_FLAGS", 0)));
    g_get_enum_preference_func = reinterpret_cast<GetEnumPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_ENUMS", 0)));
    g_get_number_preference_func = reinterpret_cast<GetNumberPreferenceFn>(
        PY4GW::Scanner::ToFunctionStart(PY4GW::Scanner::FindUseOfString("pref < PREF_VALUES", 0)));

    WarnIfMissingAddress("GetStringPreference_Func", reinterpret_cast<uintptr_t>(g_get_string_preference_func));
    WarnIfMissingAddress("GetFlagPreference_Func", reinterpret_cast<uintptr_t>(g_get_flag_preference_func));
    WarnIfMissingAddress("GetEnumPreference_Func", reinterpret_cast<uintptr_t>(g_get_enum_preference_func));
    WarnIfMissingAddress("GetNumberPreference_Func", reinterpret_cast<uintptr_t>(g_get_number_preference_func));

    const uintptr_t enum_info = FindPatternAddress("ui.enum_preference_info");
    const uintptr_t value_info = FindPatternAddress("ui.number_preference_info");
    if (enum_info) {
        g_enum_preference_options_addr = *reinterpret_cast<EnumPreferenceInfo**>(enum_info);
        WarnIfMissingAddress("EnumPreferenceOptions_Addr", reinterpret_cast<uintptr_t>(g_enum_preference_options_addr));
    } else {
        Logger::Instance().LogWarning("EnumPreferenceOptions_Ref is null.", "ui");
    }
    if (value_info) {
        g_number_preference_options_addr = *reinterpret_cast<NumberPreferenceInfo**>(value_info);
        WarnIfMissingAddress("NumberPreferenceOptions_Addr", reinterpret_cast<uintptr_t>(g_number_preference_options_addr));
    } else {
        Logger::Instance().LogWarning("NumberPreferenceOptions_Ref is null.", "ui");
    }
    return true;
}

inline bool ResolvePreferenceWriters() {
    CrashContextScope context("startup", "ui", "resolve_preference_writers");

    const uintptr_t set_string_anchor = FindPatternAddress("ui.set_string_preference");
    if (set_string_anchor) {
        g_set_string_preference_func = reinterpret_cast<SetStringPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(set_string_anchor));
    } else {
        Logger::Instance().LogWarning("SetStringPreference_Anchor is null.", "ui");
    }

    const uintptr_t quality_anchor = FindPatternAddress("ui.preference_quality_anchor");
    if (quality_anchor) {
        g_set_enum_preference_func = reinterpret_cast<SetEnumPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x8D));
        g_set_flag_preference_func = reinterpret_cast<SetFlagPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x3B));
        g_set_number_preference_func = reinterpret_cast<SetNumberPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x6A));
        g_set_in_game_static_preference_func = reinterpret_cast<SetInGameStaticPreferenceFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0xFF));
        g_trigger_terrain_rerender_func = reinterpret_cast<TriggerTerrainRerenderFn>(PY4GW::Scanner::FunctionFromNearCall(quality_anchor - 0x36));
    } else {
        Logger::Instance().LogWarning("PreferenceQuality_Anchor is null.", "ui");
    }

    const uintptr_t shadow_anchor = FindPatternAddress("ui.set_in_game_shadow_quality");
    if (shadow_anchor) {
        g_set_in_game_shadow_quality_func = reinterpret_cast<SetInGameShadowQualityFn>(PY4GW::Scanner::ToFunctionStart(shadow_anchor));
    } else {
        Logger::Instance().LogWarning("SetInGameShadowQuality_Anchor is null.", "ui");
    }

    const uintptr_t ui_scale_anchor = FindPatternAddress("ui.set_in_game_ui_scale");
    if (ui_scale_anchor) {
        g_set_in_game_ui_scale_func = reinterpret_cast<SetInGameUIScaleFn>(PY4GW::Scanner::FunctionFromNearCall(ui_scale_anchor));
    } else {
        Logger::Instance().LogWarning("SetInGameUIScale_Anchor is null.", "ui");
    }

    const uintptr_t volume_anchor = FindPatternAddress("ui.set_volume");
    if (volume_anchor) {
        g_set_volume_func = reinterpret_cast<SetVolumeFn>(PY4GW::Scanner::ToFunctionStart(volume_anchor));
    } else {
        Logger::Instance().LogWarning("SetVolume_Anchor is null.", "ui");
    }

    const uintptr_t master_volume_anchor = FindPatternAddress("ui.set_master_volume");
    if (master_volume_anchor) {
        g_set_master_volume_func = reinterpret_cast<SetMasterVolumeFn>(PY4GW::Scanner::ToFunctionStart(master_volume_anchor));
    } else {
        Logger::Instance().LogWarning("SetMasterVolume_Anchor is null.", "ui");
    }

    const uintptr_t get_renderer_anchor = FindPatternAddress("ui.get_graphics_renderer_value");
    if (get_renderer_anchor) {
        g_get_graphics_renderer_value_func = reinterpret_cast<GetGraphicsRendererValueFn>(PY4GW::Scanner::FunctionFromNearCall(get_renderer_anchor));
    } else {
        Logger::Instance().LogWarning("GetGraphicsRendererValue_Anchor is null.", "ui");
    }

    const uintptr_t set_renderer_anchor = FindPatternAddress("ui.set_graphics_renderer_value");
    if (set_renderer_anchor) {
        g_set_graphics_renderer_value_func = reinterpret_cast<SetGraphicsRendererValueFn>(PY4GW::Scanner::ToFunctionStart(set_renderer_anchor));
    } else {
        Logger::Instance().LogWarning("SetGraphicsRendererValue_Anchor is null.", "ui");
    }

    const uintptr_t set_game_renderer_mode_anchor = FindPatternAddress("ui.set_game_renderer_mode");
    if (set_game_renderer_mode_anchor) {
        g_set_game_renderer_mode_func = reinterpret_cast<SetGameRendererModeFn>(PY4GW::Scanner::FunctionFromNearCall(set_game_renderer_mode_anchor));
    } else {
        Logger::Instance().LogWarning("SetGameRendererMode_Anchor is null.", "ui");
    }

    const uintptr_t game_renderer_metrics_anchor = FindPatternAddress("ui.game_renderer_metrics");
    if (game_renderer_metrics_anchor) {
        g_get_game_renderer_mode_func = reinterpret_cast<GetGameRendererModeFn>(PY4GW::Scanner::FunctionFromNearCall(game_renderer_metrics_anchor - 0x1D));
        g_get_game_renderer_metric_func = reinterpret_cast<GetGameRendererMetricFn>(PY4GW::Scanner::FunctionFromNearCall(game_renderer_metrics_anchor - 0x5));
    } else {
        Logger::Instance().LogWarning("GameRendererMetrics_Anchor is null.", "ui");
    }

    const uintptr_t command_line_number_anchor = FindPatternAddress("ui.command_line_number");
    if (command_line_number_anchor) {
        g_command_line_number_buffer = *reinterpret_cast<uint32_t**>(command_line_number_anchor + 0x29);
        if (g_command_line_number_buffer) {
            g_command_line_number_buffer += 0x30;
        }
    } else {
        Logger::Instance().LogWarning("CommandLineNumber_Anchor is null.", "ui");
    }
    WarnIfMissingAddress("SetStringPreference_Func", reinterpret_cast<uintptr_t>(g_set_string_preference_func));
    WarnIfMissingAddress("SetEnumPreference_Func", reinterpret_cast<uintptr_t>(g_set_enum_preference_func));
    WarnIfMissingAddress("SetFlagPreference_Func", reinterpret_cast<uintptr_t>(g_set_flag_preference_func));
    WarnIfMissingAddress("SetNumberPreference_Func", reinterpret_cast<uintptr_t>(g_set_number_preference_func));
    WarnIfMissingAddress("SetInGameStaticPreference_Func", reinterpret_cast<uintptr_t>(g_set_in_game_static_preference_func));
    WarnIfMissingAddress("TriggerTerrainRerender_Func", reinterpret_cast<uintptr_t>(g_trigger_terrain_rerender_func));
    WarnIfMissingAddress("SetInGameShadowQuality_Func", reinterpret_cast<uintptr_t>(g_set_in_game_shadow_quality_func));
    WarnIfMissingAddress("SetInGameUIScale_Func", reinterpret_cast<uintptr_t>(g_set_in_game_ui_scale_func));
    WarnIfMissingAddress("SetVolume_Func", reinterpret_cast<uintptr_t>(g_set_volume_func));
    WarnIfMissingAddress("SetMasterVolume_Func", reinterpret_cast<uintptr_t>(g_set_master_volume_func));
    WarnIfMissingAddress("GetGraphicsRendererValue_Func", reinterpret_cast<uintptr_t>(g_get_graphics_renderer_value_func));
    WarnIfMissingAddress("SetGraphicsRendererValue_Func", reinterpret_cast<uintptr_t>(g_set_graphics_renderer_value_func));
    WarnIfMissingAddress("SetGameRendererMode_Func", reinterpret_cast<uintptr_t>(g_set_game_renderer_mode_func));
    WarnIfMissingAddress("GetGameRendererMode_Func", reinterpret_cast<uintptr_t>(g_get_game_renderer_mode_func));
    WarnIfMissingAddress("GetGameRendererMetric_Func", reinterpret_cast<uintptr_t>(g_get_game_renderer_metric_func));
    WarnIfMissingAddress("CommandLineNumber_Buffer", reinterpret_cast<uintptr_t>(g_command_line_number_buffer));

    return true;
}

inline bool ResolveCommandLineFunctions() {
    CrashContextScope context("startup", "ui", "resolve_command_line_functions");

    const uintptr_t address = FindPatternAddress("ui.build_login_struct_callsite");
    if (!address) {
        Logger::Instance().LogWarning("BuildLoginStruct_Callsite is null.", "ui");
        return false;
    }
    const uintptr_t login_func = PY4GW::Scanner::FunctionFromNearCall(address);
    if (login_func) {
        g_get_command_line_flag_func = reinterpret_cast<GetFlagPreferenceFn>(
            PY4GW::Scanner::FunctionFromNearCall(login_func + 0xF));
        g_get_command_line_string_func = reinterpret_cast<GetStringPreferenceFn>(
            PY4GW::Scanner::FunctionFromNearCall(login_func + 0x32));
    }
    WarnIfMissingAddress("GetCommandLineFlag_Func", reinterpret_cast<uintptr_t>(g_get_command_line_flag_func));
    WarnIfMissingAddress("GetCommandLineString_Func", reinterpret_cast<uintptr_t>(g_get_command_line_string_func));
    return true;
}

inline bool TryResolveTypedComponentCallbacks() {
    CrashContextScope context("runtime", "ui", "resolve_typed_component_callbacks");
    if (g_typed_component_callbacks_initialized) {
        return true;
    }

    const uintptr_t button_addr = FindPatternAddress("ui.button_frame_callback");
    if (button_addr) {
        g_button_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(button_addr, 0xFF));
    }

    const uintptr_t ctl_btn_addr = FindPatternAddress("ui.ctl_button_proc_callback");
    if (ctl_btn_addr) {
        g_ctl_button_proc_callback = reinterpret_cast<UIInteractionCallback>(ctl_btn_addr);
    }

    const uintptr_t text_btn_addr = FindPatternAddress("ui.text_button_frame_callback");
    if (text_btn_addr) {
        g_text_button_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(text_btn_addr, 0x20));
    }

    const uintptr_t text_label_addr = FindPatternAddress("ui.text_label_frame_callback");
    if (text_label_addr) {
        g_text_label_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(text_label_addr, 0xFFF));
    }

    const uintptr_t scrollable_addr = FindPatternAddress("ui.scrollable_frame_callback");
    if (scrollable_addr) {
        g_scrollable_frame_callback = reinterpret_cast<UIInteractionCallback>(*reinterpret_cast<const uintptr_t*>(scrollable_addr));
    }

    const uintptr_t frame_list_addr = FindPatternAddress("ui.frame_list_callback");
    if (frame_list_addr) {
        g_frame_list_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(frame_list_addr, 0xFFF));
    }

    const uintptr_t dropdown_addr = FindPatternAddress("ui.dropdown_frame_callback");
    if (dropdown_addr) {
        g_dropdown_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(dropdown_addr, 0xFFF));
    }

    const uintptr_t slider_addr = FindPatternAddress("ui.slider_frame_callback");
    if (slider_addr) {
        g_slider_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(slider_addr, 0xFFF));
    }

    const uintptr_t slider_wrapper_addr = FindPatternAddress("ui.slider_frame_wrapper_callback");
    if (slider_wrapper_addr) {
        g_slider_frame_wrapper_callback = reinterpret_cast<UIInteractionCallback>(slider_wrapper_addr);
    }

    const uintptr_t editable_addr = FindPatternAddress("ui.editable_text_frame_callback");
    if (editable_addr) {
        g_editable_text_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(editable_addr, 0xFFF));
    }

    const uintptr_t progress_addr = FindPatternAddress("ui.progress_bar_callback");
    if (progress_addr) {
        g_progress_bar_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(progress_addr, 0xFFF));
    }

    const uintptr_t tabs_addr = FindPatternAddress("ui.tabs_frame_callback");
    if (tabs_addr) {
        g_tabs_frame_callback = reinterpret_cast<UIInteractionCallback>(PY4GW::Scanner::ToFunctionStart(tabs_addr, 0xFFF));
    }

    g_typed_component_callbacks_initialized = true;
    return true;
}

bool Init();
void EnableHooks();
void DisableHooks();
void Exit();
bool WaitForUiHooksToDrain();

void __cdecl OnSendUIMessage(UIMessage message_id, void* wparam, void* lparam);
void OnOpenTemplateUiMessage(PY4GW::HookStatus* hook_status, UIMessage msgid, void* wparam, void*);
void __cdecl OnSendFrameUIMessageById(uint32_t frame_id, UIMessage message_id, void* wparam, void* lparam);
void __fastcall OnSendFrameUIMessage(GW::GWArray<UIInteractionCallback>* frame_callbacks, void*, UIMessage message_id, void* wparam, void* lparam);
uint32_t __cdecl OnCreateUIComponent(uint32_t frame_id, uint32_t component_flags, uint32_t tab_index, void* event_callback, wchar_t* name_enc, wchar_t* component_label);

}  // namespace GW::ui
