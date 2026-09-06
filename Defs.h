#pragma once

#include "obse/PluginAPI.h"
#include "obse/GameAPI.h"
#include "obse/GameActorValues.h"
#include "obse/GameData.h"
#include "obse/GameForms.h"
#include "obse/GameObjects.h"
#include "obse/FunctionScripts.h"
#include "obse/GameProcess.h"
#include "obse/GameTiles.h"
#include "obse_common/SafeWrite.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <windows.h>
#include <tlhelp32.h>
#include <cctype>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

extern IDebugLog gLog;
extern PluginHandle g_pluginHandle;
extern OBSESerializationInterface* g_serialization;

namespace TCS
{
	static constexpr UInt32 kPluginVersion = 1;

	static constexpr UInt32 kSaveVersion = 3;

	static constexpr UInt32 kRecordState = ('T') | ('C' << 8) | ('S' << 16) | ('S' << 24);

	static constexpr UInt32 kMaxSkillLevel = 100;
	static constexpr float kProgressEpsilon = 0.0001f;

	static constexpr UInt32 kMaxCustomSkills = 64;

	static constexpr UInt32 kNativeClassMajorCount = 7;

	static constexpr UInt32 kFirstNativeSkillAV = 0x0C;

	static constexpr UInt32 kLastNativeSkillAV = 0x20;

	static constexpr UInt32 kSentinelMajorAV = 0xFFFFFFFF;

	static constexpr UInt32 kClassMenuCommit = 0x005973F0;

	static constexpr UInt32 kClassMenuRefreshDetails = 0x00596CF0;

	static constexpr UInt32 kSkillsMenuPreselect = 0x005D5D40;

	static constexpr UInt32 kSkillsMenuUpdateAccept = 0x005D5AB0;

	static constexpr UInt32 kSkillsMenuDetails = 0x005D5B40;

	static constexpr UInt32 kSkillsMenuAccept = 0x005D5E50;

	static constexpr UInt32 kSkillsMenuClose = 0x005D5720;

	static constexpr UInt32 kStatsMenuCreateRows = 0x005DC630;

	static constexpr UInt32 kStatsMenuRefresh = 0x005DA1A0;

	static constexpr UInt32 kStatsMenuDetails = 0x005DBBD0;

	static constexpr UInt32 kTESOnIdleIOManagerCallSite = 0x0040D8D8;

	static constexpr UInt32 kIOManagerProcessThreads = 0x00433590;

	static constexpr UInt32 kStatsMenuCreateRowsCall = 0x005DCCA3;

	static constexpr UInt32 kStatsMenuRefreshCalls[] =
	{
		0x0057A78F, 0x005DC8CB, 0x005DCC9C, 0x005DCE34, 0x005DCEE5, 0x005DCEFC
	};

	static constexpr UInt32 kStatsMenuSummaryTileOffset = 0x30;

	static constexpr UInt32 kStatsMenuSkillParentOffset = 0x3C;

	static constexpr UInt32 kStatsMenuSkillRowsOffset = 0x60;

	static constexpr UInt32 kStatsRowSyntheticSkillIdTrait = kTileValue_user22;

	static constexpr UInt32 kStatsRowSyntheticMarkerTrait = kTileValue_user23;

	static constexpr const char* kStatsSkillTemplate = "stat_skill_template";

	static constexpr UInt32 kMenuCreateTileFromTemplate = 0x00585410;

	static constexpr UInt32 kSkillsMenuCreateSkillRow = 0x005D6270;

	static constexpr UInt32 kTileSetFloat = 0x0058CEB0;

	static constexpr UInt32 kTileSetString = 0x0058CED0;

	static constexpr UInt32 kTileGetFloat = 0x00588BD0;

	static constexpr UInt32 kActorValueGetName = 0x00565CC0;
	using ActorValueGetNameFn = const char* (__cdecl*)(UInt32 av);

	static constexpr UInt32 kPlayerIncrementAttributeBonusBucket = 0x006648D0;
	using PlayerIncrementAttributeBonusBucketFn = void(__thiscall*)(PlayerCharacter* player, UInt32 attributeAV);

	static constexpr UInt32 kPlayerMaybeStartNextAttributeBonusBucket = 0x0065FB30;
	using PlayerMaybeStartNextAttributeBonusBucketFn = void(__thiscall*)(PlayerCharacter* player);

	static constexpr UInt32 kTileAnimateTrait = 0x00589980;
	using TileAnimateTraitFn = void(__thiscall*)(Tile* tile, UInt32 trait, float from, float duration, float to);

	static constexpr UInt32 kAVTokenRegisterRVA = 0x1001F420 - 0x10000000;
	using AVTokenRegisterFn = bool(__thiscall*)(void* avToken);

	static constexpr UInt32 kAVTokenLookupNextRVA = 0x1001F160 - 0x10000000;
	using AVTokenLookupNextFn = void* (__cdecl*)(const void* current);

	static constexpr UInt32 kXSkillsLinkFormExRVA = 0x10019F80 - 0x10000000;
	static constexpr UInt32 kTESSkillExActorValueOffset = 0x2C;
	using LinkFormExFn = char(__thiscall*)(void* skillForm);

	static constexpr UInt32 kXSkillsSkillMapRVA = 0x10053430 - 0x10000000;
	static constexpr UInt32 kXSkillsFormMapRVA = 0x10053624 - 0x10000000;

	static constexpr UInt32 kXSkillsTreeFindRVA = 0x10017050 - 0x10000000;
	using XSkillsTreeFindFn = void** (__thiscall*)(void* treeHeader, void** outSlot, const UInt32* key);

	static constexpr UInt32 kXSkillsRecordProgressOffset = 0x18;
	static constexpr UInt32 kXSkillsRecordRequiredProgressOffset = 0x1C;

	static constexpr UInt32 kTESDescriptionGetTextVtableSlot = 0xA54EF0;
	using TESDescriptionGetTextFn = const char* (__thiscall*)(void* thisDescription, TESForm* parentForm, UInt32 recordCode);

	static constexpr UInt32 kTESSkillGetMasteryDescription = 0x0052EAB0;
	static constexpr UInt32 kTESSkillGetMasteryDescriptionPatchLength = 8;
	static const UInt8 kTESSkillGetMasteryDescriptionExpected[kTESSkillGetMasteryDescriptionPatchLength] =
	{
		0x8B, 0x54, 0x24, 0x04, // mov edx, [esp+4]
		0x85, 0xD2,             // test edx, edx
		0x75, 0x08,             // jnz short +8
	};
	using TESSkillGetMasteryDescriptionFn = const char* (__thiscall*)(void* thisForm, UInt32 masteryLevel);

	static constexpr UInt32 kOpenSkillPerkMenu = 0x0057B370;
	typedef UInt32(__cdecl* _OpenSkillPerkMenu)(const char* xmlName, UInt32 arg1, UInt32 arg2, UInt32 arg3, ...);
	static constexpr UInt32 kGenericMenuArgInt = 0;
	static constexpr UInt32 kGenericMenuArgFloat = 1;
	static constexpr UInt32 kGenericMenuArgString = 2;
	static constexpr UInt32 kGenericMenuArgEnd = 3;
	static constexpr const char* kSkillPerkMenuXml = "skill_perk.xml";
	static constexpr const char* kSkillPerkOkText = "OK";

	enum ActorValueGroups
	{
		kAVGroup_Attribute = 0x0, // 0x00 - 0x07
		kAVGroup_Stat = 0x1,      // 0x08 - 0x0B
		kAVGroup_Skill = 0x2,     // 0x0C - 0x20
		kAVGroup_AI = 0x3,        // 0x21 - 0x24
		kAVGroup_Social = 0x4,    // 0x25 - 0x27
		kAVGroup_Misc = 0x5,      // 0x28 - 0x29
		kAVGroup_Combat = 0x6,    // 0x2A+
		kAVGroup__MAX = 0x7,
	};

	static constexpr UInt32 kClassMenuTileOffset = 0x04;

	static constexpr UInt32 kClassMenuSelectedClassOffset = 0x3C;

	static constexpr UInt32 kClassMenuCustomClassOffset = 0x40;

	static constexpr UInt32 kClassMenuStepOffset = 0x58;

	static constexpr UInt32 kClassMenuSelectedSkillsOffset = 0x68;

	static constexpr UInt32 kSkillsMenuTileOffset = 0x04;

	static constexpr UInt32 kSkillsMenuListTileOffset = 0x28;

	static constexpr UInt32 kSkillsMenuAcceptButtonOffset = 0x34;

	static constexpr UInt32 kSkillsMenuModeOffset = 0x3C;

	static constexpr UInt32 kSkillsMenuCurrentValueOffset = 0x40;

	static constexpr UInt32 kSkillsMenuSelectionCapOffset = 0x44;

	static constexpr UInt32 kSkillsMenuSelectedTileOffset = 0x48;

	static constexpr UInt32 kSkillsMenuClassMenuOffset = 0x4C;

	enum class XPCurveMode : UInt32
	{
		kVanilla = 0,
		kLinear = 1,
		kCustom = 2,
	};

	struct SkillDefinition
	{
		UInt32 skillId;
		std::string editorId;
		std::string name;
		std::string description;
		std::string iconLarge;
		std::string iconSmall;
		UInt32 governingAttributeAV;
		UInt32 specialization;
		XPCurveMode xpCurve;
		UInt32 realActorValue = 0;
		bool isOwnForm = false;
		void* xSkillsForm = nullptr;
		std::string apprenticeText;
		std::string journeymanText;
		std::string expertText;
		std::string masterText;
	};

	struct SkillState
	{
		UInt32 level;
		float progress;
		float requiredProgress;
		UInt32 levelUps;
		UInt32 governingAttributeIncreaseCount;
		UInt8 major;
		UInt8 padding[3];
	};

	using ClassMenuCommitFn = void(__thiscall*)(void* classMenu);

	struct StagedMajorSelection
	{
		UInt32 nativeActorValues[kNativeClassMajorCount];
		UInt32 nativeCount;
	};

	struct StagedSyntheticSelection
	{
		UInt32 skillIds[kNativeClassMajorCount];
		UInt32 skillCount;
	};


	using ClassMenuRefreshDetailsFn = void(__thiscall*)(void* classMenu, void* displayedClass);

	static constexpr UInt32 kClassMenuRefreshDetailsCalls[] =
	{
		0x00597682,
		0x00597121,
		0x00597189,
		0x005971C9,
		0x005974C1,
	};

	static constexpr UInt32 kSkillsMenuOpenEntry = 0x005D6390;

	static constexpr UInt32 kSkillsMenuOpenEntryPatchLength = 5;

	static const UInt8 kSkillsMenuOpenEntryExpected[kSkillsMenuOpenEntryPatchLength] =
	{
		0x83, 0xEC, 0x10,
		0x53,
		0x55,
	};

	static constexpr UInt32 kSkillsMenuOpenXmlPathPush = 0x005D63CD;

	static constexpr UInt32 kSkillsMenuOpenXmlPathContinue = 0x005D63D2;

	static constexpr UInt32 kNotClassCreationMode = 0xFFFFFFFF;

	static const char kNativeSkillsMenuXmlPath[] = "Data\\Menus\\CharGen\\skills_menu.xml";

	using SkillsMenuCreateSkillRowFn = Tile * (__thiscall*)(void* skillsMenu, const char* displayName, UInt32 rowValue);

	static constexpr UInt32 kPickerRowValueTrait = kTileValue_user2;

	static constexpr UInt32 kPickerRowSelectedTrait = kTileValue_user3;

	static constexpr UInt32 kPickerSyntheticSkillIdTrait = kTileValue_user22;

	static constexpr UInt32 kPickerSyntheticMarkerTrait = kTileValue_user23;

	static constexpr UInt32 kPickerRowPlaceholderNativeValue = kActorVal_Luck; // still used as the safe default/fallback value where a specific skill index isn't available
	static constexpr UInt32 kSafeMarkerAVs[] = {
		kActorVal_Strength, kActorVal_Intelligence, kActorVal_Willpower, kActorVal_Agility,
		kActorVal_Speed, kActorVal_Endurance, kActorVal_Personality, kActorVal_Luck,
		kActorVal_Health, kActorVal_Magicka, kActorVal_Fatigue, kActorVal_Encumbrance,
		// kActorVal_Armorer..kActorVal_Speechcraft (12-32) deliberately skipped — the real skill AV range
		kActorVal_Aggression, kActorVal_Confidence, kActorVal_Energy, kActorVal_Responsibility,
		kActorVal_Bounty, kActorVal_Fame, kActorVal_Infamy, kActorVal_MagickaMultiplier,
		kActorVal_NightEyeBonus, kActorVal_AttackBonus, kActorVal_DefendBonus, kActorVal_CastingPenalty,
		kActorVal_Blindness, kActorVal_Chameleon, kActorVal_Invisibility, kActorVal_Paralysis,
		kActorVal_Silence, kActorVal_Confusion, kActorVal_DetectItemRange, kActorVal_SpellAbsorbChance,
		kActorVal_SpellReflectChance, kActorVal_SwimSpeedMultiplier, kActorVal_WaterBreathing, kActorVal_WaterWalking,
		kActorVal_StuntedMagicka, kActorVal_DetectLifeRange, kActorVal_ReflectDamage, kActorVal_Telekinesis,
		kActorVal_ResistFire, kActorVal_ResistFrost, kActorVal_ResistDisease, kActorVal_ResistMagic,
		kActorVal_ResistNormalWeapons, kActorVal_ResistParalysis, kActorVal_ResistPoison, kActorVal_ResistShock,
		kActorVal_Vampirism, kActorVal_Darkness, kActorVal_ResistWaterDamage,
	};
	static constexpr UInt32 kSafeMarkerAVCount = sizeof(kSafeMarkerAVs) / sizeof(kSafeMarkerAVs[0]);
	static UInt32 GetPickerRowPlaceholderAV(UInt32 skillIndex)
	{
		return kSafeMarkerAVs[skillIndex % kSafeMarkerAVCount];
	}

	struct PickerRow
	{
		Tile* tile;
	};

	static constexpr const char* kSkillsDirectory = "Data\\OBSE\\Plugins\\TrueCustomSkills\\";

	using SkillsMenuPreselectFn = void(__thiscall*)(void* skillsMenu);

	static constexpr UInt32 kSkillsMenuPreselectCall = 0x005D65E3;

	using SkillsMenuAcceptFn = void(__thiscall*)(void* skillsMenu, UInt32 buttonId, Tile* tile);

	static constexpr UInt32 kSkillsMenuAcceptPatchLength = 14;

	static const UInt8 kSkillsMenuAcceptExpected[kSkillsMenuAcceptPatchLength] =
	{
		0x6A, 0xFF,
		0x68, 0xD8, 0x5B, 0x9B, 0x00,
		0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
		0x50
	};

	using SkillsMenuDetailsFn = void(__thiscall*)(void* skillsMenu, void* explicitValue);

	static constexpr UInt32 kSkillsMenuDetailsPatchLength = 12;

	static const UInt8 kSkillsMenuDetailsExpected[kSkillsMenuDetailsPatchLength] =
	{
		0x8B, 0x44, 0x24, 0x04, // mov eax, [esp+4]
		0x83, 0xF8, 0xFF,       // cmp eax, -1
		0x53,                   // push ebx
		0x56,                   // push esi
		0x57,                   // push edi
		0x8B, 0xF1              // mov esi, ecx
	};

	static constexpr UInt32 kStatsMenuDetailsPatchLength = 32;

	static const UInt8 kStatsMenuDetailsExpected[kStatsMenuDetailsPatchLength] =
	{
		0x6A, 0xFF,                         // push -1
		0x68, 0xF0, 0x20, 0x9C, 0x00,       // push offset SEH_5DBBD0
		0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, // mov eax, fs:[0]
		0x50,                               // push eax
		0x83, 0xEC, 0x38,                   // sub esp, 0x38
		0xA1, 0xAC, 0x0A, 0xB3, 0x00,       // mov eax, [0x00B30AAC]
		0x33, 0xC4,                         // xor eax, esp
		0x89, 0x44, 0x24, 0x34,             // mov [esp+0x34], eax
		0x53,                               // push ebx
		0x55,                               // push ebp
		0x56,                               // push esi
		0x57                                // push edi
	};

	static constexpr UInt32 kStatsMenuDetailTileOffset = 0x58;

	using MenuCreateTileFromTemplateFn = Tile * (__thiscall*)(void* menu, Tile* parent, const char* templateName, UInt32 unk);

	struct StatsRow
	{
		Tile* tile;
		bool isFallback;
	};

	static constexpr UInt32 kStatSkillScrollBarId = 32;

	static constexpr UInt32 kStatSkillWindowPaneId = 31;

	static constexpr float kDarNSkillRowSpacing = 27.0f;

	using MenuQueInsertXMLFn = void(__cdecl*)(void* targetTile, const char* xml, UInt8 flag);

	static constexpr UInt32 kMenuQueInsertXMLRVA = 0x100040B0 - 0x10000000;

	using StatsMenuCreateRowsFn = void(__thiscall*)(void* statsMenu);
	using IOManagerProcessThreadsFn = void(__thiscall*)(void* ioManager);

	using StatsMenuRefreshFn = void(__thiscall*)(void* statsMenu, UInt32 actorValue);

	extern SkillDefinition g_skills[kMaxCustomSkills];
	extern UInt32 g_skillCount;
	extern SkillState g_states[kMaxCustomSkills];
	static constexpr UInt8 kPostLoadPushDelay = 2;
	extern UInt8 g_postLoadPushCountdown[kMaxCustomSkills];
	void NormalizeState(UInt32 index);
	float GetProgressFraction(UInt32 index);
	extern UInt32 g_appliedPatches;
	bool LooseTextureAssetExists(const std::string& relativePath);
	extern UInt32 g_failedPatches;
	bool WriteRelCallChained(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget, UInt32& originalTarget);
	bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength = 5);
	bool InstallFunctionJumpHook(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength, void*& original);
	[[nodiscard]] UInt32 __stdcall DetourVtable(UInt32 addr, UInt32 dst);
	void SetTileString(Tile* tile, UInt32 trait, const char* value);
	float GetTileFloat(Tile* tile, UInt32 trait);
	void SetTileFloat(Tile* tile, UInt32 trait, float value);
	Tile* FindChildTileById(Tile* parent, UInt32 id);
	Tile* FindDescendantTileById(Tile* root, UInt32 id, UInt32 maxDepth = 6);
	extern MenuQueInsertXMLFn g_menuQueInsertXML;
	bool ResolveMenuQueInsertXML();
	extern void* g_avTokenRegister;
	extern void* g_avTokenLookupNext;
	bool ResolveAddActorValues();
	extern void* g_xSkillsLinkFormEx;
	bool ResolveXSkillsLinkFormEx();
	bool LinkSkillWithXSkills(UInt32 avCode, const char* skillName);
	extern void* g_xSkillsTreeFind;
	extern void* g_xSkillsSkillMap;
	bool ResolveXSkillsSkillMap();
	extern void* g_xSkillsFormMap;
	bool ResolveXSkillsFormMap();
	bool ReadXSkillsProgress(UInt32 avCode, float& outProgress, float& outRequired);
	bool WriteXSkillsProgress(UInt32 avCode, float progress, float required);
	bool ReadXSkillsSkillCode(UInt32 avCode, UInt8& outSkillCode);
	bool SetXSkillsGoverningAttributeAndSpecialization(UInt32 avCode, UInt32 governingAttribute, UInt32 specialization);
	bool SetXSkillsIcon(UInt32 avCode, const std::string& iconPath);
	void* GetXSkillsFormForAV(UInt32 avCode);
	bool InstallHooks();
	UInt32 GetSkillIndexById(UInt32 skillId);
	const char* GetSkillIconLarge(UInt32 index);
	const char* GetSkillIconSmall(UInt32 index);
	void ComposeSkillDescriptionText(UInt32 index, char* buffer, UInt32 bufferSize);
	UInt32 RegisterCustomActorValue(const std::string& skillName, bool& outWasReused);
	void ReconcileSkillLevelWithRealAV(UInt32 index);
	void PushSkillLevelToRealAV(UInt32 index);
	void ForceSetSkillLevelOnRealAV(UInt32 index);
	UInt32 GetRealAVLevel(UInt32 index);
	void ReconcileSkillProgressWithXSkills(UInt32 index);
	void ApplyMajorSpecializationScaling(UInt32 index);
	void EnsureCustomActorValuesRegistered();
	void LoadSkillDefinitionsFromDisk();
	void RegisterSerializationCallbacks();
}