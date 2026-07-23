#pragma once

#include <cstdint>

namespace GW::Constants {

// UI message ids used by the Guild Wars frame/UI system. Every value here is the
// game engine's own runtime id, not a locally-assigned number, so entries are
// never renumbered - a new one is added by dropping in the engine's real id.
//
// The high bits pick the *kind* of message (this is the engine's own scheme):
//   0x00000000  frame messages   - incoming low-level UI (input, layout, lifecycle)
//   0x10000000  kHighBitBase     - incoming UI-state notifications (agents, party, ...)
//   0x30000000  send band        - outgoing action requests (kSend*)
//
// Within each band the entries below are grouped by subsystem and kept ascending
// by value inside each group. Ordering is cosmetic (values are explicit); group
// membership is the only thing that carries meaning.
enum class UIMessage : uint32_t {

    // ---- Frame band (0x0) : low-level frame/widget messages ----

    // Frame lifecycle
    kNone = 0x0,
    kResize = 0x8,
    kInitFrame = 0x9,
    kDestroyFrame = 0xb,

    // Input (keyboard / mouse)
    kKeyDown = 0x20,
    kSetFocus = 0x21,
    kKeyUp = 0x22,
    kMouseClick = 0x24,
    kMouseCoordsClick = 0x26,
    kMouseUp = 0x28,
    kToggleButtonDown = 0x2E,
    kMouseClick2 = 0x31,
    kMouseAction = 0x32,

    // Layout / content
    kSetLayout = 0x37,
    kMeasureContent = 0x38,
    kRefreshContent = 0x3B,

    // Skill-list frame
    // Frame-level "add skill to skill list" message. Used by the skill_list_filter
    // listener to intercept skills being added to the tome / skill-trainer /
    // capture windows (legacy GWToolbox GameSettings::OnSkillList_UICallback).
    // Value is 0x57; GWCA labels this kFrameMessage_0x47, but that name is a
    // historical index that does not match the runtime id, so it is not reused here.
    kSkillListAddSkill = 0x57,

    // ---- Notification band (0x10000000) : incoming UI-state notifications ----

    kHighBitBase = 0x10000000,

    // Agents & targeting
    kRerenderAgentModel = 0x10000007,
    kAgentDestroy = 0x10000008,
    kUpdateAgentEffects = 0x10000009,
    kAgentSpeechBubble = 0x10000017,
    kShowAgentNameTag = 0x10000019,
    kHideAgentNameTag = 0x1000001A,
    kSetAgentNameTagAttribs = 0x1000001B,
    kSetAgentProfession = 0x1000001D,
    kChangeTarget = 0x10000020,
    kAgentSkillActivated = 0x10000024,
    kAgentSkillActivatedInstantly = 0x10000025,
    kAgentSkillCancelled = 0x10000026,
    kAgentStartCasting = 0x10000027,
    kCalledTargetChange = 0x10000115,

    // Map / world / instance
    kShowMapEntryMessage = 0x10000029,
    kSetCurrentPlayerData = 0x1000002A,
    kPostProcessingEffect = 0x10000034,
    kMapLoaded = 0x1000008C,
    kLoadMapContext = 0x10000098,
    kCompassDraw = 0x1000009E,
    kStartMapLoad = 0x100000C2,
    kWorldMapUpdated = 0x100000C7,
    kMapChange = 0x10000111,

    // Heroes
    kHeroAgentAdded = 0x10000038,
    kHeroDataAdded = 0x10000039,
    kHideHeroPanel = 0x100001A2,
    kShowHeroPanel = 0x100001A3,

    // Storage / chest
    kShowXunlaiChest = 0x10000040,

    // Combat state (minions / morale)
    kMinionCountUpdated = 0x10000046,
    kMoraleChange = 0x10000047,

    // Effects
    kEffectAdd = 0x10000055,
    kEffectRenew = 0x10000056,
    kEffectRemove = 0x10000057,

    // Skills & skillbar
    kSkillActivated = 0x1000005b,
    kUpdateSkillbar = 0x1000005E,
    kUpdateSkillsAvailable = 0x1000005f,
    kTomeSkillSelection = 0x10000103,
    kCollapseExpandSkillListSection = 0x100001C6,

    // Titles & experience
    kPlayerTitleChanged = 0x10000064,
    kTitleProgressUpdated = 0x10000065,
    kExperienceGained = 0x10000066,

    // Chat
    kWriteToChatLog = 0x1000007F,
    kWriteToChatLogWithSender = 0x10000080,
    kAllyOrGuildMessage = 0x10000081,
    kPlayerChatMessage = 0x10000082,
    kOpenWhisper = 0x10000092,
    kAppendMessageToChat = 0x10000194,

    // Friends & guild
    kFriendUpdated = 0x1000008B,
    kGuildMemberUpdated = 0x100000DA,
    kGuildHall = 0x10000180,
    kLeaveGuildHall = 0x10000182,

    // Windows / on-screen messages / hints / errors
    kFloatingWindowMoved = 0x10000084,
    kOnScreenMessage = 0x100000A2,
    kShowHint = 0x100000E1,
    kErrorMessage = 0x10000119,

    // Dialog
    kDialogButton = 0x100000A3,
    kDialogBody = 0x100000A6,

    // Party targeting
    kTargetNPCPartyMember = 0x100000B3,
    kTargetPlayerPartyMember = 0x100000B4,

    // Vendor / merchant
    kVendorWindow = 0x100000B5,
    kVendorItems = 0x100000B9,
    kVendorTransComplete = 0x100000BB,
    kVendorQuote = 0x100000BD,

    // Weapon sets
    kWeaponSetSwapComplete = 0x100000E9,
    kWeaponSetSwapCancel = 0x100000EA,
    kWeaponSetUpdated = 0x100000EB,

    // Gold
    kUpdateGoldCharacter = 0x100000EC,
    kUpdateGoldStorage = 0x100000ED,

    // Inventory & equipment
    kInventorySlotUpdated = 0x100000EE,
    kEquipmentSlotUpdated = 0x100000EF,
    kInventorySlotCleared = 0x100000F1,
    kEquipmentSlotCleared = 0x100000F2,
    kGetInventoryAgentId = 0x100001A7,
    kInventoryRelated1 = 0x100001A8,
    kInventoryRelated2 = 0x100001A9,
    kInventoryRelated3 = 0x100001AA,
    kEquipItem = 0x100001AB,
    kMoveItem = 0x100001AC,
    kInventoryAgentChanged = 0x100001C1,
    kInventoryRelated_1 = 0x100001C2,
    kInventoryRelated_2 = 0x100001C3,

    // Items
    kPreStartSalvage = 0x10000102,
    kItemUpdated = 0x10000106,
    kRedrawItem = 0x10000174,
    kItemRelated_1 = 0x100001AD,
    kItemTooltip = 0x100001AE,
    kItemRelated_3 = 0x100001AF,
    kItemRelated_4 = 0x100001B0,

    // PvP
    kPvPWindowContent = 0x100000FA,

    // Trade
    kTradePlayerUpdated = 0x10000105,
    kTradeSessionStart = 0x10000165,
    kTradeSessionUpdated = 0x1000016b,
    kInitiateTrade = 0x100001B1,

    // Party management
    kPartyHardModeChanged = 0x1000011A,
    kPartyAddHenchman = 0x1000011B,
    kPartyRemoveHenchman = 0x1000011C,
    kPartyAddHero = 0x1000011E,
    kPartyRemoveHero = 0x1000011F,
    kPartyAddPlayer = 0x10000124,
    kPartyRemovePlayer = 0x10000126,
    kDisableEnterMissionBtn = 0x1000012A,
    kShowCancelEnterMissionBtn = 0x1000012D,
    kPartyDefeated = 0x1000012F,
    kPartySearchInviteReceived = 0x10000137,
    kPartySearchInviteSent = 0x10000139,
    kPartyShowConfirmDialog = 0x1000013A,

    // Preferences, settings & UI position
    kPreferenceEnumChanged = 0x10000140,
    kPreferenceFlagChanged = 0x10000141,
    kPreferenceValueChanged = 0x10000142,
    kUIPositionChanged = 0x10000143,
    kToggleOptionsWindow = 0x1000016F,
    kCheckUIState = 0x10000176,
    kCloseSettings = 0x10000177,
    kChangeSettingsTab = 0x10000178,
    kDestroyUIPositionOverlay = 0x1000017D,
    kEnableUIPositionOverlay = 0x1000017E,

    // Session / login
    kLoginStateChanged = 0x10000050,
    kLogout = 0x1000009D,
    kPreBuildLoginScene = 0x10000144,
    kTriggerLogoutPrompt = 0x1000016E,
    kSetPreGameContext_Value0 = 0x10000187,
    kGetPreGameContext_Value0 = 0x10000189,
    kSetPreGameContext_Value1 = 0x1000018A,
    kGetPreGameContext_Value1 = 0x1000018B,

    // Quests
    kQuestAdded = 0x1000014E,
    kQuestDetailsChanged = 0x1000014F,
    kQuestRemoved = 0x10000150,
    kClientActiveQuestChanged = 0x10000151,
    kServerActiveQuestChanged = 0x10000153,
    kUnknownQuestRelated = 0x10000154,

    // Objectives & mission completion
    kDungeonComplete = 0x10000156,
    kMissionComplete = 0x10000157,
    kVanquishComplete = 0x10000159,
    kObjectiveAdd = 0x1000015A,
    kObjectiveComplete = 0x1000015B,
    kObjectiveUpdated = 0x1000015C,
    kMissionStatusRelated = 0x100001C4,

    // Travel & external
    kTravel = 0x10000183,
    kOpenWikiUrl = 0x10000184,

    // Templates
    kTemplateRelated_1 = 0x100001C7,
    kTemplateRelated_2 = 0x100001C8,
    kPromptSaveTemplate = 0x100001C9,
    kOpenTemplate = 0x100001CA,
    kTemplateRelated_3 = 0x100001CB,
    kTemplateRelated_4 = 0x100001CC,

    // Unclassified
    kUnused_1c2 = 0x100001C5,

    // ---- Send band (0x30000000) : outgoing action requests ----

    // Party / mission
    kSendEnterMission = 0x30000000 | 0x2,
    kSendSetActiveQuest = 0x30000000 | 0x9,
    kSendAbandonQuest = 0x30000000 | 0xA,

    // Skillbar / weapon set
    kSendLoadSkillbar = 0x30000000 | 0x3,
    kSendPingWeaponSet = 0x30000000 | 0x4,

    // Items / inventory
    kSendMoveItem = 0x30000000 | 0x5,
    kSendUseItem = 0x30000000 | 0x8,
    kIdentifyItem = 0x30000000 | 0x22,

    // Merchant
    kSendMerchantRequestQuote = 0x30000000 | 0x6,
    kSendMerchantTransactItem = 0x30000000 | 0x7,

    // Targeting & world interaction
    kSendChangeTarget = 0x30000000 | 0xB,
    kSendMoveToWorldPoint = 0x30000000 | 0xC,
    kSendInteractNPC = 0x30000000 | 0xD,
    kSendInteractGadget = 0x30000000 | 0xE,
    kSendInteractItem = 0x30000000 | 0xF,
    kSendInteractEnemy = 0x30000000 | 0x10,
    kSendInteractPlayer = 0x30000000 | 0x11,
    kSendCallTarget = 0x30000000 | 0x13,
    kSendWorldAction = 0x30000000 | 0x20,

    // Dialog
    kSendAgentDialog = 0x30000000 | 0x14,
    kSendGadgetDialog = 0x30000000 | 0x15,
    kSendDialog = 0x30000000 | 0x16,

    // Chat / whisper
    kStartWhisper = 0x30000000 | 0x17,
    kGetSenderColor = 0x30000000 | 0x18,
    kGetMessageColor = 0x30000000 | 0x19,
    kSendChatMessage = 0x30000000 | 0x1B,
    kLogChatMessage = 0x30000000 | 0x1D,
    kRecvWhisper = 0x30000000 | 0x1E,
    kPrintChatMessage = 0x30000000 | 0x1F,

    // Renderer
    kSetRendererValue = 0x30000000 | 0x21
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
    AutoTournPartySort,
    ChatState,
    ChatTab,
    DistrictLastVisitedLanguage,
    DistrictLastVisitedLanguage2,
    DistrictLastVisitedNonInternationalLanguage,
    DistrictLastVisitedNonInternationalLanguage2,
    DamageTextSize,
    FullscreenGamma,
    InventoryBag,
    TextLanguage,
    AudioLanguage,
    ChatFilterLevel,
    RefreshRate,
    ScreenSizeX,
    ScreenSizeY,
    SkillListFilterRarity,
    SkillListSortMethod,
    SkillListViewMode,
    SoundQuality,
    StorageBagPage,
    Territory,
    TextureQuality,
    UseBestTextureFiltering,
    EffectsVolume,
    DialogVolume,
    BackgroundVolume,
    MusicVolume,
    UIVolume,
    Vote,
    WindowPosX,
    WindowPosY,
    WindowSizeX,
    WindowSizeY,
    SealedSeed,
    SealedCount,
    FieldOfView,
    CameraRotationSpeed,
    ScreenBorderless,
    MasterVolume,
    ClockMode,
    MobileUiScale,
    GamepadCursorSpeed,
    LastLoginMethod,
    Count = 44
};

enum class FlagPreference : uint32_t {
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
    ShowSpendAttributesButton = 0x31,
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

enum class TooltipType : uint32_t {
    None = 0x0,
    EncString1 = 0x4,
    EncString2 = 0x6,
    Item = 0x8,
    WeaponSet = 0xC,
    Skill = 0x14,
    Attribute = 0x4000
};

enum class FrameChild : uint32_t {
    FirstChild = 0,
    LastChild = 1,
    NextSibling = 2,
    PrevSibling = 3
};

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

}  // namespace GW::Constants
