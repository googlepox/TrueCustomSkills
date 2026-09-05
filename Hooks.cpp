#include "Defs.h"

namespace TCS
{

	static void FillNativeClassMajorArraySafe(UInt32* nativeSkills, const UInt32* selectedNativeActorValues, UInt32 selectedNativeCount)
	{
		UInt32 written = 0;
		for (UInt32 i = 0; i < selectedNativeCount && written < kNativeClassMajorCount; ++i)
			nativeSkills[written++] = selectedNativeActorValues[i];

		for (; written < kNativeClassMajorCount; ++written)
			nativeSkills[written] = kSentinelMajorAV;
	}

	static UInt32 g_classMenuCommitOriginalTarget = kClassMenuCommit;

	static ClassMenuCommitFn ClassMenuCommitOriginal()
	{
		return reinterpret_cast<ClassMenuCommitFn>(g_classMenuCommitOriginalTarget);
	}

	static UInt32* GetClassMenuNativeSkillArray(void* classMenu)
	{
		return classMenu ? reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuSelectedSkillsOffset) : nullptr;
	}

	static StagedMajorSelection g_stagedSelection = {};

	static StagedSyntheticSelection g_stagedSynthetic = {};

	static void __fastcall HookClassMenuCommit(void* classMenu, void*)
	{
		if (g_stagedSynthetic.skillCount > 0)
		{
			void* customClass = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuCustomClassOffset);
			if (customClass)
			{
				for (UInt32 slot = 0; slot < kNativeClassMajorCount; ++slot)
					*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(customClass) + 0x44 + slot * sizeof(UInt32)) = kSentinelMajorAV;
			}
		}

		ClassMenuCommitOriginal()(classMenu);

		if (g_stagedSynthetic.skillCount == 0)
			return;

		UInt32* nativeSkills = GetClassMenuNativeSkillArray(classMenu);
		if (nativeSkills)
		{
			FillNativeClassMajorArraySafe(nativeSkills, g_stagedSelection.nativeActorValues, g_stagedSelection.nativeCount);
			_MESSAGE("TCS: wrote majorSkills[7] native=%u sentinel-filled=%u",
				g_stagedSelection.nativeCount,
				kNativeClassMajorCount - g_stagedSelection.nativeCount);
		}

		for (UInt32 i = 0; i < g_stagedSynthetic.skillCount; ++i)
		{
			const UInt32 index = GetSkillIndexById(g_stagedSynthetic.skillIds[i]);
			if (index < g_skillCount)
			{
				g_states[index].major = 1;
				NormalizeState(index);
				const UInt32 realLevel = GetRealAVLevel(index);
				UInt32 base = g_states[index].level;
				if (realLevel > base)
					base = realLevel;
				UInt32 target = base + 20;
				if (target > kMaxSkillLevel)
					target = kMaxSkillLevel;
				_MESSAGE("TCS: major-skill-bonus for skillId=%u ownStateLevel=%u realAVLevel=%u base=%u -> target=%u",
					g_skills[index].skillId, g_states[index].level, realLevel, base, target);
				g_states[index].level = target;

				PushSkillLevelToRealAV(index);
			}
		}
	}

	static UInt32 g_classMenuRefreshDetailsOriginalTarget = kClassMenuRefreshDetails;

	static ClassMenuRefreshDetailsFn ClassMenuRefreshDetailsOriginal()
	{
		return reinterpret_cast<ClassMenuRefreshDetailsFn>(g_classMenuRefreshDetailsOriginalTarget);
	}

	static Tile* GetClassMenuTile(void* classMenu)
	{
		return classMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuTileOffset) : nullptr;
	}

	static void* GetClassMenuSelectedClass(void* classMenu)
	{
		return classMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuSelectedClassOffset) : nullptr;
	}

	static void* GetClassMenuCustomClass(void* classMenu)
	{
		return classMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(classMenu) + kClassMenuCustomClassOffset) : nullptr;
	}

	static void* GetSkillsMenuClassMenu(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuClassMenuOffset) : nullptr;
	}

	static UInt32 GetSkillsMenuMode(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuModeOffset) : 0xFFFFFFFF;
	}

	static bool IsClassSkillPicker(void* skillsMenu)
	{
		return GetSkillsMenuClassMenu(skillsMenu) != nullptr && GetSkillsMenuMode(skillsMenu) == 0;
	}

	static Tile* GetSkillsMenuListTile(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuListTileOffset) : nullptr;
	}

	static Tile* GetSkillsMenuTile(void* skillsMenu)
	{
		return skillsMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuTileOffset) : nullptr;
	}

	static Tile* GetSkillsMenuSelectedRowTile(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return nullptr;

		const UInt32 headNode = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		if (!headNode || !*reinterpret_cast<UInt32*>(headNode + 8))
			return nullptr;

		const UInt32 selectedNode = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		if (!selectedNode)
			return nullptr;

		return *reinterpret_cast<Tile**>(selectedNode + 8);
	}

	static void SetSkillsMenuSelectionCap(void* skillsMenu, UInt32 cap)
	{
		if (skillsMenu)
			*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(skillsMenu) + kSkillsMenuSelectionCapOffset) = cap;
	}

	static void ApplySyntheticMajorDisplay(void* classMenu, void* displayedClass)
	{
		if (!displayedClass)
			displayedClass = GetClassMenuSelectedClass(classMenu);
		if (GetClassMenuCustomClass(classMenu) && displayedClass != GetClassMenuCustomClass(classMenu))
			return;

		Tile* tile = GetClassMenuTile(classMenu);
		if (!tile || g_stagedSynthetic.skillCount == 0)
			return;

		const UInt32 firstSyntheticSlot = g_stagedSelection.nativeCount;
		for (UInt32 i = 0; i < g_stagedSynthetic.skillCount && (firstSyntheticSlot + i) < kNativeClassMajorCount; ++i)
		{
			const UInt32 slot = firstSyntheticSlot + i;
			const UInt32 skillIndex = GetSkillIndexById(g_stagedSynthetic.skillIds[i]);
			const char* syntheticName = skillIndex < g_skillCount ? g_skills[skillIndex].name.c_str() : "Unknown Skill";
			SetTileString(tile, kTileValue_user1 + slot, syntheticName);
		}
	}

	static void __fastcall HookClassMenuRefreshDetails(void* classMenu, void*, void* displayedClass)
	{
		ClassMenuRefreshDetailsOriginal()(classMenu, displayedClass);
		ApplySyntheticMajorDisplay(classMenu, displayedClass);
	}

	static UInt32 g_currentSkillsMenuOpenMode = kNotClassCreationMode;

	static void* g_skillsMenuOpenEntryOriginal = nullptr;

	static void __cdecl LogSkillsMenuOpenMode()
	{
		_MESSAGE("TCS: SkillsMenu_Open called with mode=%u", g_currentSkillsMenuOpenMode);
	}

	static __declspec(naked) void HookSkillsMenuOpenEntry()
	{
		__asm
		{
			mov eax, [esp + 4]
			mov g_currentSkillsMenuOpenMode, eax
			call LogSkillsMenuOpenMode
			jmp dword ptr[g_skillsMenuOpenEntryOriginal]
		}
	}

	static const char* __cdecl ComputeSkillsMenuXmlPath()
	{
		return kNativeSkillsMenuXmlPath;
	}

	static __declspec(naked) void HookSkillsMenuOpenXmlPathPush()
	{
		__asm
		{
			push ecx
			call ComputeSkillsMenuXmlPath
			pop ecx
			push eax
			mov edx, kSkillsMenuOpenXmlPathContinue
			jmp edx
		}
	}

	static void* g_customPickerMenu = nullptr;

	static Tile* CreateSyntheticSkillRow(void* skillsMenu, const char* displayName, UInt32 rowValue)
	{
		return reinterpret_cast<SkillsMenuCreateSkillRowFn>(kSkillsMenuCreateSkillRow)(skillsMenu, displayName, rowValue);
	}

	static PickerRow g_pickerRows[kMaxCustomSkills] = {};

	static void* g_classPickerSkillsMenu = nullptr;

	static UInt32 GetPickerRowSyntheticSkillId(Tile* tile)
	{
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) != 2.0f)
			return 0xFFFFFFFF;

		const float skillId = GetTileFloat(tile, kPickerSyntheticSkillIdTrait);
		return std::isfinite(skillId) ? static_cast<UInt32>(skillId + 0.5f) : 0xFFFFFFFF;
	}

	static UInt32 FindPickerSkillIndexByTile(Tile* tile)
	{
		return GetSkillIndexById(GetPickerRowSyntheticSkillId(tile));
	}

	static bool TryGetForeignSyntheticPickerSkillId(Tile* tile, UInt32& skillId)
	{
		skillId = 0xFFFFFFFF;
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) != 2.0f)
			return false;

		const float rawSkillId = GetTileFloat(tile, kPickerSyntheticSkillIdTrait);
		if (!std::isfinite(rawSkillId))
			return false;

		skillId = static_cast<UInt32>(rawSkillId + 0.5f);
		return GetSkillIndexById(skillId) >= g_skillCount;
	}

	static bool HasPickerRow(void* skillsMenu, UInt32 index)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return false;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (FindPickerSkillIndexByTile(tile) == index)
				return true;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return false;
	}

	static bool IsNativePickerSkillRow(Tile* tile, UInt32& actorValue)
	{
		if (!tile || GetTileFloat(tile, kPickerSyntheticMarkerTrait) == 2.0f)
			return false;

		const float av = GetTileFloat(tile, kPickerRowValueTrait);
		actorValue = std::isfinite(av) ? static_cast<UInt32>(av + 0.5f) : 0xFFFFFFFF;
		return actorValue >= kFirstNativeSkillAV && actorValue <= kLastNativeSkillAV;
	}

	struct PickerSortEntry
	{
		const char* name;
		Tile* nativeTile;
		UInt32 ourIndex; 
	};

	static void UpdatePickerRow(void* skillsMenu, UInt32 index, UInt32 listIndex);

	static void RepairClassPickerRowOrderingInterleaved(void* skillsMenu, UInt32& outCombinedCount)
	{
		outCombinedCount = 0;
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		static constexpr UInt32 kMaxNativeSkillRows = 21;
		static constexpr UInt32 kMaxCombinedEntries = kMaxNativeSkillRows + kMaxCustomSkills;
		PickerSortEntry entries[kMaxCombinedEntries];
		UInt32 entryCount = 0;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node && entryCount < kMaxNativeSkillRows)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 actorValue = 0xFFFFFFFF;
			if (IsNativePickerSkillRow(tile, actorValue))
			{
				const char* name = reinterpret_cast<ActorValueGetNameFn>(kActorValueGetName)(actorValue);
				entries[entryCount].name = (name && name[0]) ? name : "";
				entries[entryCount].nativeTile = tile;
				entries[entryCount].ourIndex = 0;
				++entryCount;
			}
			node = *reinterpret_cast<UInt32*>(node + 4);
		}

		for (UInt32 i = 0; i < g_skillCount && entryCount < kMaxCombinedEntries; ++i)
		{
			entries[entryCount].name = g_skills[i].name.c_str();
			entries[entryCount].nativeTile = nullptr;
			entries[entryCount].ourIndex = i;
			++entryCount;
		}

		for (UInt32 a = 1; a < entryCount; ++a)
		{
			const PickerSortEntry key = entries[a];
			SInt32 b = static_cast<SInt32>(a) - 1;
			while (b >= 0 && _stricmp(entries[b].name, key.name) > 0)
			{
				entries[b + 1] = entries[b];
				--b;
			}
			entries[b + 1] = key;
		}

		for (UInt32 rank = 0; rank < entryCount; ++rank)
		{
			if (entries[rank].nativeTile)
				SetTileFloat(entries[rank].nativeTile, kTileValue_listindex, static_cast<float>(rank));
			else
				UpdatePickerRow(skillsMenu, entries[rank].ourIndex, rank);
		}

		outCombinedCount = entryCount;
	}

	static UInt32 CountForeignSyntheticPickerRows(void* skillsMenu)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return 0;

		UInt32 count = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticPickerSkillId(tile, skillId))
				++count;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return count;
	}

	static void RepairForeignSyntheticPickerOrdering(void* skillsMenu, UInt32 combinedNativeAndOursCount)
	{
		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		UInt32 foreignIndex = 0;
		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			UInt32 skillId = 0;
			if (TryGetForeignSyntheticPickerSkillId(tile, skillId))
				SetTileFloat(tile, kTileValue_listindex, static_cast<float>(combinedNativeAndOursCount + foreignIndex++));

			node = *reinterpret_cast<UInt32*>(node + 4);
		}
	}

	static void UpdatePickerRow(void* skillsMenu, UInt32 index, UInt32 listIndex)
	{
		Tile* row = g_pickerRows[index].tile;
		if (!row)
			return;

		SetTileFloat(row, kPickerRowSelectedTrait, 1.0f);
		SetTileFloat(row, kPickerSyntheticSkillIdTrait, static_cast<float>(g_skills[index].skillId));
		SetTileFloat(row, kPickerSyntheticMarkerTrait, 2.0f);
		SetTileFloat(row, kTileValue_listindex, static_cast<float>(listIndex));
	}

	static void SyncClassSkillPickerRows(void* skillsMenu)
	{

		if (!IsClassSkillPicker(skillsMenu))
			return;

		EnsureCustomActorValuesRegistered();

		if (g_classPickerSkillsMenu != skillsMenu)
		{
			g_classPickerSkillsMenu = skillsMenu;
			std::memset(g_pickerRows, 0, sizeof(g_pickerRows));
		}

		SetSkillsMenuSelectionCap(skillsMenu, kNativeClassMajorCount);

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!g_pickerRows[i].tile && !HasPickerRow(skillsMenu, i))
				g_pickerRows[i].tile = CreateSyntheticSkillRow(skillsMenu, g_skills[i].name.c_str(), GetPickerRowPlaceholderAV(i));
		}

		UInt32 combinedCount = 0;
		RepairClassPickerRowOrderingInterleaved(skillsMenu, combinedCount);
		const UInt32 foreignSyntheticCount = CountForeignSyntheticPickerRows(skillsMenu);
		RepairForeignSyntheticPickerOrdering(skillsMenu, combinedCount);

		_MESSAGE("TCS: synced picker rows combined-native-and-ours=%u foreign-synthetic=%u ours=%u", combinedCount, foreignSyntheticCount, g_skillCount);
	}

	static UInt32 g_skillsMenuPreselectOriginalTarget = kSkillsMenuPreselect;

	static SkillsMenuPreselectFn SkillsMenuPreselectOriginal()
	{
		return reinterpret_cast<SkillsMenuPreselectFn>(g_skillsMenuPreselectOriginalTarget);
	}

	static void __fastcall HookSkillsMenuPreselect(void* skillsMenu, void*)
	{
		SkillsMenuPreselectOriginal()(skillsMenu);
		SyncClassSkillPickerRows(skillsMenu);
	}

	static void CommitCustomPickerSelection(void* classMenu, const UInt32* nativeActorValues, UInt32 nativeCount, const UInt32* skillIds, UInt32 skillCount)
	{
		g_stagedSelection.nativeCount = nativeCount < kNativeClassMajorCount ? nativeCount : kNativeClassMajorCount;
		std::memcpy(g_stagedSelection.nativeActorValues, nativeActorValues, sizeof(UInt32) * g_stagedSelection.nativeCount);

		g_stagedSynthetic.skillCount = skillCount < kNativeClassMajorCount ? skillCount : kNativeClassMajorCount;
		std::memcpy(g_stagedSynthetic.skillIds, skillIds, sizeof(UInt32) * g_stagedSynthetic.skillCount);

		_MESSAGE("TCS: staged selection native=%u synthetic=%u", g_stagedSelection.nativeCount, g_stagedSynthetic.skillCount);
	}

	static bool RowIsSelected(Tile* tile)
	{
		return GetTileFloat(tile, kPickerRowSelectedTrait) == 2.0f;
	}

	static bool IsVisibleNativeSkillActorValue(UInt32 actorValue)
	{
		return actorValue >= kFirstNativeSkillAV && actorValue <= kLastNativeSkillAV;
	}

	static bool AddUniqueUInt32(UInt32* values, UInt32& count, UInt32 maxCount, UInt32 value)
	{
		for (UInt32 i = 0; i < count; ++i)
		{
			if (values[i] == value)
				return false;
		}
		if (count >= maxCount)
			return false;

		values[count++] = value;
		return true;
	}

	static void CollectSelectedClassPickerRows(
		void* skillsMenu,
		UInt32* nativeActorValues,
		UInt32& nativeCount,
		UInt32& foreignSyntheticSelectedCount,
		UInt32* selectedSyntheticSkillIds,
		UInt32& selectedSyntheticSkillCount)
	{
		nativeCount = 0;
		foreignSyntheticSelectedCount = 0;
		selectedSyntheticSkillCount = 0;

		Tile* listTile = GetSkillsMenuListTile(skillsMenu);
		if (!listTile)
			return;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(listTile) + 0x38);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile && RowIsSelected(tile))
			{
				const UInt32 ourIndex = FindPickerSkillIndexByTile(tile);
				if (ourIndex < g_skillCount)
				{
					AddUniqueUInt32(selectedSyntheticSkillIds, selectedSyntheticSkillCount,
						kNativeClassMajorCount, g_skills[ourIndex].skillId);
				}
				else
				{
					UInt32 foreignSkillId = 0;
					if (TryGetForeignSyntheticPickerSkillId(tile, foreignSkillId))
					{
						++foreignSyntheticSelectedCount;
						AddUniqueUInt32(selectedSyntheticSkillIds, selectedSyntheticSkillCount,
							kNativeClassMajorCount, foreignSkillId);
					}
					else
					{
						const UInt32 actorValue = static_cast<UInt32>(GetTileFloat(tile, kPickerRowValueTrait) + 0.5f);
						if (IsVisibleNativeSkillActorValue(actorValue))
							AddUniqueUInt32(nativeActorValues, nativeCount, kNativeClassMajorCount, actorValue);
					}
				}
			}

			node = *reinterpret_cast<UInt32*>(node + 4);
		}
	}

	static void* g_skillsMenuAcceptOriginal = nullptr;

	static SkillsMenuAcceptFn SkillsMenuAcceptOriginal()
	{
		return reinterpret_cast<SkillsMenuAcceptFn>(g_skillsMenuAcceptOriginal);
	}

	static void __fastcall HookSkillsMenuAccept(void* skillsMenu, void*, UInt32 buttonId, Tile* tile)
	{
		bool haveCorrection = false;
		void* classMenu = nullptr;
		UInt32 nativeActorValues[kNativeClassMajorCount] = {};
		UInt32 nativeCount = 0;

		if (IsClassSkillPicker(skillsMenu))
		{
			UInt32 foreignSyntheticSelectedCount = 0;
			UInt32 selectedSyntheticSkillIds[kNativeClassMajorCount] = {};
			UInt32 selectedSyntheticSkillCount = 0;

			CollectSelectedClassPickerRows(skillsMenu, nativeActorValues, nativeCount,
				foreignSyntheticSelectedCount, selectedSyntheticSkillIds, selectedSyntheticSkillCount);

			classMenu = GetSkillsMenuClassMenu(skillsMenu);
			CommitCustomPickerSelection(classMenu,
				nativeActorValues, nativeCount, selectedSyntheticSkillIds, selectedSyntheticSkillCount);
			haveCorrection = true;
		}

		SkillsMenuAcceptOriginal()(skillsMenu, buttonId, tile);

		if (haveCorrection)
		{
			UInt32* nativeSkills = GetClassMenuNativeSkillArray(classMenu);
			if (nativeSkills)
				FillNativeClassMajorArraySafe(nativeSkills, nativeActorValues, nativeCount);
		}
	}

	static void* g_skillsMenuDetailsOriginal = nullptr;

	static SkillsMenuDetailsFn SkillsMenuDetailsOriginal()
	{
		return reinterpret_cast<SkillsMenuDetailsFn>(g_skillsMenuDetailsOriginal);
	}

	static void __fastcall HookSkillsMenuDetails(void* skillsMenu, void*, UInt32 explicitValue)
	{
		SkillsMenuDetailsOriginal()(skillsMenu, reinterpret_cast<void*>(explicitValue));

		const UInt32 mode = GetSkillsMenuMode(skillsMenu);

		if (mode != 0)
			return;

		UInt32 skillIndex = 0xFFFFFFFF;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (GetPickerRowPlaceholderAV(i) == explicitValue)
			{
				skillIndex = i;
				break;
			}
		}
		if (skillIndex < g_skillCount)
			_MESSAGE("TCS: HookSkillsMenuDetails mode=%u explicitValue=%u skillIndex=%u g_skillCount=%u",
				mode, explicitValue, skillIndex, g_skillCount);

		if (skillIndex >= g_skillCount)
			return;

		Tile* detailsTile = GetSkillsMenuTile(skillsMenu);
		if (!detailsTile)
		{
			_MESSAGE("TCS: HookSkillsMenuDetails no detailsTile");
			return;
		}

		const char* iconPath = GetSkillIconLarge(skillIndex);
		const char* detailText = g_skills[skillIndex].description.c_str();
		SetTileString(detailsTile, kTileValue_user1, detailText);
		SetTileString(detailsTile, kTileValue_user2, iconPath);
	}

	static Tile* CreateTileFromTemplate(void* menu, Tile* parent, const char* templateName)
	{
		if (!menu || !parent)
			return nullptr;
		return reinterpret_cast<MenuCreateTileFromTemplateFn>(kMenuCreateTileFromTemplate)(menu, parent, templateName, 0);
	}

	static Tile* GetStatsMenuSummaryTile(void* statsMenu)
	{
		return statsMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSummaryTileOffset) : nullptr;
	}

	static Tile* GetStatsMenuSkillParent(void* statsMenu)
	{
		return statsMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillParentOffset) : nullptr;
	}

	static StatsRow g_statsRows[kMaxCustomSkills] = {};

	static void* g_statsMenu = nullptr;

	static bool RowIsOurStatsSkill(Tile* tile, UInt32 index)
	{
		if (!tile || GetTileFloat(tile, kStatsRowSyntheticMarkerTrait) != 2.0f)
			return false;

		const float skillId = GetTileFloat(tile, kStatsRowSyntheticSkillIdTrait);
		return std::isfinite(skillId) && index < g_skillCount &&
			static_cast<UInt32>(skillId + 0.5f) == g_skills[index].skillId;
	}

	static UInt32 FindStatsSkillIndexByTile(Tile* tile)
	{
		if (!tile || GetTileFloat(tile, kStatsRowSyntheticMarkerTrait) != 2.0f)
			return 0xFFFFFFFF;

		const float skillId = GetTileFloat(tile, kStatsRowSyntheticSkillIdTrait);
		if (!std::isfinite(skillId))
			return 0xFFFFFFFF;

		return GetSkillIndexById(static_cast<UInt32>(skillId + 0.5f));
	}

	static Tile* GetStatsMenuDetailTile(void* statsMenu)
	{
		return statsMenu ? *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuDetailTileOffset) : nullptr;
	}

	static Tile* FindStatsRow(void* statsMenu, UInt32 index)
	{
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		if (!parent)
			return nullptr;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (RowIsOurStatsSkill(tile, index))
				return tile;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return nullptr;
	}

	static Tile* FindXSkillsRowForSkill(Tile* parent, UInt8 skillCode, Tile* excludeTile)
	{
		if (!parent || skillCode == 0)
			return nullptr;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile && tile != excludeTile)
			{
				const float rowValue = GetTileFloat(tile, kTileValue_user6);
				const bool isMatch = std::isfinite(rowValue) && static_cast<UInt32>(rowValue + 0.5f) == static_cast<UInt32>(skillCode);
				if (isMatch)
					return tile;
			}
			node = *reinterpret_cast<UInt32*>(node);
		}
		return nullptr;
	}

	static float ComputeDarNSkillScrollMax(Tile* scrollBar, UInt32 finalMajorCount, UInt32 nativeMinorRows)
	{
		UInt32 ourMinorCount = 0;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!g_states[i].major)
				++ourMinorCount;
		}

		const float itemsVisible = GetTileFloat(scrollBar, kTileValue_user8);
		const float totalRows = 1.0f
			+ static_cast<float>(finalMajorCount)
			+ static_cast<float>(nativeMinorRows)
			+ static_cast<float>(ourMinorCount);

		float scrollMax = totalRows - (finalMajorCount == 0 ? 1.0f : 0.0f) - itemsVisible;
		if (scrollMax < 0.0f)
			scrollMax = 0.0f;
		return scrollMax;
	}

	static void FixDarNSkillScrollRange(Tile* summary, UInt32 finalMajorCount, UInt32 nativeMinorRows)
	{
		Tile* scrollBar = FindChildTileById(summary, kStatSkillScrollBarId);
		if (!scrollBar)
			return;

		const float scrollMax = ComputeDarNSkillScrollMax(scrollBar, finalMajorCount, nativeMinorRows);
		SetTileFloat(scrollBar, kTileValue_user2, scrollMax);
		_MESSAGE("TCS: scroll range finalMajorCount=%u nativeMinorRows=%u scrollMax=%.1f readback=%.1f",
			finalMajorCount, nativeMinorRows, scrollMax, GetTileFloat(scrollBar, kTileValue_user2));
	}

	static bool g_menuQueYFormulaApplied = false;

	static float g_menuQueLastAppliedScrollMax = -1.0f;

	static float MeasureNativeRowSpacing(void* statsMenu);

	static void ApplyMenuQueDarNScrollbarPatch(void* statsMenu, Tile* pane, Tile* scrollBar, UInt32 finalMajorCount, UInt32 nativeMinorRows)
	{
		if (!pane || !scrollBar)
			return;
		if (!ResolveMenuQueInsertXML())
			return;

		if (!g_menuQueYFormulaApplied)
		{
			char yFormula[256] = {};
			_snprintf_s(yFormula, sizeof(yFormula), _TRUNCATE,
				" <y> <copy>0</copy> <sub src=\"stat_p3_scroll_bar\" trait=\"user7\"/> <mul>%d</mul> </y> ",
				static_cast<int>(MeasureNativeRowSpacing(statsMenu)));
			g_menuQueInsertXML(pane, yFormula, 0);
			g_menuQueYFormulaApplied = true;
		}

		const float scrollMax = ComputeDarNSkillScrollMax(scrollBar, finalMajorCount, nativeMinorRows);
		if (scrollMax != g_menuQueLastAppliedScrollMax)
		{
			const float previous = g_menuQueLastAppliedScrollMax;
			char user2Literal[64] = {};
			_snprintf_s(user2Literal, sizeof(user2Literal), _TRUNCATE, " <user2> %d </user2> ", static_cast<int>(scrollMax));
			g_menuQueInsertXML(scrollBar, user2Literal, 0);
			g_menuQueLastAppliedScrollMax = scrollMax;
			_MESSAGE("TCS: applied MenuQue InsertXML user2=%d (was %d)", static_cast<int>(scrollMax), static_cast<int>(previous));
		}
	}

	static float MeasureNativeRowSpacing(void* statsMenu)
	{
		for (UInt32 i = 0; i < 21; ++i)
		{
			Tile* row = *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillRowsOffset + i * sizeof(Tile*));
			if (!row)
				continue;
			const float listIndex = GetTileFloat(row, kTileValue_listindex);
			if (!std::isfinite(listIndex) || listIndex <= 0.0f)
				continue;
			const float y = GetTileFloat(row, kTileValue_y);
			if (std::isfinite(y) && y != 0.0f)
				return y / listIndex;
		}
		_MESSAGE("TCS: MeasureNativeRowSpacing found no usable row — falling back to DarN's own constant (%.1f)", kDarNSkillRowSpacing);
		return kDarNSkillRowSpacing;
	}

	static void RepositionDarNSkillPane(void* statsMenu, Tile* summary)
	{
		Tile* scrollBar = FindChildTileById(summary, kStatSkillScrollBarId);
		Tile* pane = FindDescendantTileById(summary, kStatSkillWindowPaneId);
		if (!g_menuQueYFormulaApplied)
			_MESSAGE("TCS: RepositionDarNSkillPane lookup scrollBar=%p pane=%p", (void*)scrollBar, (void*)pane);
		if (!scrollBar || !pane)
			return;

		if (g_menuQueYFormulaApplied)
			return;

		SetTileFloat(pane, kTileValue_target, 1.0f);

		const float scrollValue = GetTileFloat(scrollBar, kTileValue_user7);
		const float newY = -scrollValue * MeasureNativeRowSpacing(statsMenu);
		SetTileFloat(pane, kTileValue_y, newY);
		_MESSAGE("TCS: pane reposition scroll=%.2f newY=%.2f readback=%.2f", scrollValue, newY, GetTileFloat(pane, kTileValue_y));
	}

	static UInt32 CountNativeStatsRows(void* statsMenu, bool wantMajor, UInt32 nativeMajorCount)
	{
		UInt32 count = 0;
		for (UInt32 i = 0; i < 21; ++i)
		{
			Tile* row = *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillRowsOffset + i * sizeof(Tile*));
			if (!row)
				continue;
			const float order = GetTileFloat(row, kTileValue_listindex);
			const bool isMajor = std::isfinite(order) && order >= 0.0f && order < static_cast<float>(nativeMajorCount);
			if (isMajor == wantMajor)
				++count;
		}
		return count;
	}

	static void LogNativeStatsRowState(void* statsMenu, UInt32 nativeMajorCount)
	{
		UInt32 nullRows = 0;
		float maxMinorListIndex = -1.0f;
		char dump[512] = {};
		size_t dumpUsed = 0;
		for (UInt32 i = 0; i < 21; ++i)
		{
			Tile* row = *reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillRowsOffset + i * sizeof(Tile*));
			if (!row)
			{
				++nullRows;
				continue;
			}
			const float order = GetTileFloat(row, kTileValue_listindex);
			if (std::isfinite(order) && order >= static_cast<float>(nativeMajorCount) && order > maxMinorListIndex)
				maxMinorListIndex = order;
			int written = _snprintf_s(dump + dumpUsed, sizeof(dump) - dumpUsed, _TRUNCATE, "%s%u:%.0f", dumpUsed ? "," : "", i, order);
			if (written > 0)
				dumpUsed += static_cast<size_t>(written);
		}
		_MESSAGE("TCS: native row state nullRows=%u maxMinorListIndex=%.1f rows[i:listindex]=%s", nullRows, maxMinorListIndex, dump);
	}

	static void UpdateStatsRowTile(UInt32 index, Tile* tile, float order)
	{
		if (!tile || index >= g_skillCount)
			return;

		NormalizeState(index);
		const SkillState& state = g_states[index];
		SetTileFloat(tile, kTileValue_user1, 1.0f);
		SetTileFloat(tile, kTileValue_user2, GetProgressFraction(index));
		SetTileFloat(tile, kTileValue_user3, static_cast<float>(state.level));
		SetTileString(tile, kTileValue_user4, g_skills[index].name.c_str());
		SetTileString(tile, kTileValue_user5, GetSkillIconSmall(index));
		UInt8 skillCode = 0;
		ReadXSkillsSkillCode(g_skills[index].realActorValue, skillCode);
		SetTileFloat(tile, kTileValue_user6, static_cast<float>(skillCode));
		SetTileFloat(tile, kTileValue_user7, static_cast<float>(state.level));
		SetTileFloat(tile, kTileValue_listindex, order);
		SetTileFloat(tile, kStatsRowSyntheticSkillIdTrait, static_cast<float>(g_skills[index].skillId));
		SetTileFloat(tile, kStatsRowSyntheticMarkerTrait, 2.0f);
	}

	static StatsMenuRefreshFn StatsMenuRefreshOriginal();

	static void SyncStatsMenuRows(void* statsMenu)
	{
		Tile* parent = GetStatsMenuSkillParent(statsMenu);
		Tile* summary = GetStatsMenuSummaryTile(statsMenu);
		if (!parent || !summary)
			return;

		if (g_statsMenu != statsMenu)
		{
			g_statsMenu = statsMenu;
			std::memset(g_statsRows, 0, sizeof(g_statsRows));
			g_menuQueYFormulaApplied = false;
			g_menuQueLastAppliedScrollMax = -1.0f;
		}

		UInt32 nullRows = 0;
		for (UInt32 i = 0; i < 21; ++i)
		{
			if (!*reinterpret_cast<Tile**>(reinterpret_cast<UInt8*>(statsMenu) + kStatsMenuSkillRowsOffset + i * sizeof(Tile*)))
				++nullRows;
		}
		if (nullRows > 0)
			return;

		UInt32 ourMajorCount = 0;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_states[i].major)
				++ourMajorCount;
		}

		const UInt32 nativeMajorCount = kNativeClassMajorCount - ourMajorCount;
		const UInt32 nativeMinorRows = CountNativeStatsRows(statsMenu, false, nativeMajorCount);

		if (ourMajorCount)
		{
			const UInt32 shiftAmount = ourMajorCount + (nativeMajorCount == 0 ? 1 : 0);

			UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
			while (node)
			{
				Tile* tile = *reinterpret_cast<Tile**>(node + 8);
				if (tile)
				{
					bool isOurs = false;
					for (UInt32 i = 0; i < g_skillCount; ++i)
					{
						if (g_statsRows[i].tile == tile)
						{
							isOurs = true;
							break;
						}
					}
					if (!isOurs)
					{
						const float order = GetTileFloat(tile, kTileValue_listindex);
						if (std::isfinite(order) && order >= static_cast<float>(nativeMajorCount))
							SetTileFloat(tile, kTileValue_listindex, order + static_cast<float>(shiftAmount));
					}
				}
				node = *reinterpret_cast<UInt32*>(node);
			}
		}

		_MESSAGE("TCS: SyncStatsMenuRows counts nativeMajorCount=%u nativeMinorRows=%u ourMajorCount=%u",
			nativeMajorCount, nativeMinorRows, ourMajorCount);
		LogNativeStatsRowState(statsMenu, nativeMajorCount);

		UInt32 majorPlaced = 0;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_postLoadPushCountdown[i] > 0 && g_skills[i].isOwnForm && g_skills[i].realActorValue != 0)
			{
				if (--g_postLoadPushCountdown[i] == 0)
				{
					ForceSetSkillLevelOnRealAV(i);
					WriteXSkillsProgress(g_skills[i].realActorValue, g_states[i].progress, g_states[i].requiredProgress);

					StatsMenuRefreshOriginal()(statsMenu, g_skills[i].realActorValue);
				}
			}

			ReconcileSkillLevelWithRealAV(i);

			if (g_skills[i].isOwnForm && g_postLoadPushCountdown[i] == 0)
				ReconcileSkillProgressWithXSkills(i);

			if (!g_states[i].major)
				continue;

			const float order = static_cast<float>(nativeMajorCount + majorPlaced++);

			UInt8 skillCode = 0;
			Tile* xSkillsRow = nullptr;
			if (ReadXSkillsSkillCode(g_skills[i].realActorValue, skillCode))
			{
				xSkillsRow = FindXSkillsRowForSkill(parent, skillCode,
					g_statsRows[i].isFallback ? g_statsRows[i].tile : nullptr);
			}

			if (xSkillsRow)
			{
				if (g_statsRows[i].tile && g_statsRows[i].isFallback && g_statsRows[i].tile != xSkillsRow)
				{
					SetTileFloat(g_statsRows[i].tile, kTileValue_listindex, 9999.0f);
				}

				const bool tilePointerChanged = g_statsRows[i].tile && !g_statsRows[i].isFallback && g_statsRows[i].tile != xSkillsRow;
				if (tilePointerChanged)
				{
					_MESSAGE("TCS: XSKILLS TILE POINTER CHANGED for skillId=%u — was %p, now %p (xSkills is recreating rows, not reusing them)",
						g_skills[i].skillId, (void*)g_statsRows[i].tile, (void*)xSkillsRow);
				}

				SetTileFloat(xSkillsRow, kTileValue_listindex, order);
				SetTileFloat(xSkillsRow, kTileValue_user0, order);
				g_statsRows[i].tile = xSkillsRow;
				g_statsRows[i].isFallback = false;

				SetTileFloat(xSkillsRow, kStatsRowSyntheticSkillIdTrait, static_cast<float>(g_skills[i].skillId));
				SetTileFloat(xSkillsRow, kStatsRowSyntheticMarkerTrait, 2.0f);
				continue;
			}

			if (!g_statsRows[i].tile)
			{
				g_statsRows[i].tile = FindStatsRow(statsMenu, i);
				if (!g_statsRows[i].tile)
					g_statsRows[i].tile = CreateTileFromTemplate(statsMenu, parent, kStatsSkillTemplate);
				g_statsRows[i].isFallback = true;
			}
			if (!g_statsRows[i].tile)
			{
				_MESSAGE("TCS: failed to create StatsMenu row for skillId=%u", g_skills[i].skillId);
				continue;
			}
			UpdateStatsRowTile(i, g_statsRows[i].tile, order);
		}

		const UInt32 finalMajorCount = nativeMajorCount + ourMajorCount;
		if (ourMajorCount)
			SetTileFloat(summary, kTileValue_user3, static_cast<float>(finalMajorCount));

		if (Tile* pane = FindDescendantTileById(summary, kStatSkillWindowPaneId))
		{
			if (Tile* scrollBar = FindChildTileById(summary, kStatSkillScrollBarId))
				ApplyMenuQueDarNScrollbarPatch(statsMenu, pane, scrollBar, finalMajorCount, nativeMinorRows);
		}

		if (!g_menuQueInsertXML)
			FixDarNSkillScrollRange(summary, finalMajorCount, nativeMinorRows);

	}

	static UInt32 g_statsMenuCreateRowsOriginalTarget = kStatsMenuCreateRows;

	static UInt32 g_statsMenuRefreshOriginalTarget = kStatsMenuRefresh;

	static StatsMenuCreateRowsFn StatsMenuCreateRowsOriginal()
	{
		return reinterpret_cast<StatsMenuCreateRowsFn>(g_statsMenuCreateRowsOriginalTarget);
	}

	static StatsMenuRefreshFn StatsMenuRefreshOriginal()
	{
		return reinterpret_cast<StatsMenuRefreshFn>(g_statsMenuRefreshOriginalTarget);
	}

	static void __fastcall HookStatsMenuCreateRows(void* statsMenu, void*)
	{
		std::memset(g_statsRows, 0, sizeof(g_statsRows));
		g_statsMenu = statsMenu;
		StatsMenuCreateRowsOriginal()(statsMenu);

		EnsureCustomActorValuesRegistered();

		SyncStatsMenuRows(statsMenu);
	}

	static void __fastcall HookStatsMenuRefresh(void* statsMenu, void*, UInt32 actorValue)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].isOwnForm && g_skills[i].realActorValue == actorValue)
			{
				break;
			}
		}

		StatsMenuRefreshOriginal()(statsMenu, actorValue);

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].isOwnForm && g_skills[i].realActorValue == actorValue)
			{
				break;
			}
		}

		RepositionDarNSkillPane(statsMenu, GetStatsMenuSummaryTile(statsMenu));

		if (actorValue == 0xFFFFFFFF)
			SyncStatsMenuRows(statsMenu);
	}

	static void* g_statsMenuDetailsOriginal = nullptr;

	static void* g_originalTESDescriptionGetText = nullptr;

	static void* g_originalTESSkillGetMasteryDescription = nullptr;

	static const char* __fastcall HookTESSkillGetMasteryDescription(void* thisForm, void* /*unused, absorbs EDX*/, UInt32 masteryLevel)
	{
		if (masteryLevel >= 1 && masteryLevel <= 4)
		{
			for (UInt32 i = 0; i < g_skillCount; ++i)
			{
				if (g_skills[i].isOwnForm && g_skills[i].xSkillsForm == thisForm)
				{
					const std::string* masteryTexts[4] = { &g_skills[i].apprenticeText, &g_skills[i].journeymanText, &g_skills[i].expertText, &g_skills[i].masterText };
					const std::string& text = *masteryTexts[masteryLevel - 1];
					if (!text.empty())
					{
						return text.c_str();
					}
					break;
				}
			}
		}
		return reinterpret_cast<TESSkillGetMasteryDescriptionFn>(g_originalTESSkillGetMasteryDescription)(thisForm, masteryLevel);
	}

	static const char* __fastcall HookTESDescriptionGetText(void* thisDescription, void* /*unused, absorbs EDX*/, TESForm* parentForm, UInt32 recordCode)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!g_skills[i].isOwnForm || !g_skills[i].xSkillsForm)
				continue;

			UInt8* form = reinterpret_cast<UInt8*>(g_skills[i].xSkillsForm);
			if (thisDescription == form + 0x18)
			{
				return g_skills[i].description.c_str();
			}

			static constexpr UInt32 kLevelQuoteOffset = 0x40;
			static constexpr UInt32 kLevelQuoteStride = 8;
			const std::string* masteryTexts[4] = { &g_skills[i].apprenticeText, &g_skills[i].journeymanText, &g_skills[i].expertText, &g_skills[i].masterText };
			bool matchedTier = false;
			for (UInt32 tier = 0; tier < 4; ++tier)
			{
				if (thisDescription == form + kLevelQuoteOffset + kLevelQuoteStride * tier)
				{
					matchedTier = true;
					if (!masteryTexts[tier]->empty())
					{
						return masteryTexts[tier]->c_str();
					}
					break;
				}
			}

			if (!matchedTier && thisDescription != form + 0x18)
			{
				const SInt32 deltaFromForm = static_cast<SInt32>(reinterpret_cast<UInt8*>(thisDescription) - form);
			}
		}
		return reinterpret_cast<TESDescriptionGetTextFn>(g_originalTESDescriptionGetText)(thisDescription, parentForm, recordCode);
	}

	static void __stdcall HandleStatsMenuDetails(void* statsMenu, UInt32 buttonId, Tile* selectedTile)
	{
		if (buttonId != 0x22)
			return;

		const UInt32 index = FindStatsSkillIndexByTile(selectedTile);

		if (index >= g_skillCount)
			return;

		if (!g_skills[index].isOwnForm)
			return;

		Tile* detailTile = GetStatsMenuDetailTile(statsMenu);
		if (!detailTile)
		{
			_MESSAGE("TCS: HandleStatsMenuDetails no detailTile");
			return;
		}

		char detailText[768] = {};
		ComposeSkillDescriptionText(index, detailText, sizeof(detailText));

		SetTileFloat(detailTile, kTileValue_user4, 2.0f);
		SetTileString(detailTile, kTileValue_user2, GetSkillIconLarge(index));
		SetTileString(detailTile, kTileValue_user3, detailText);

		reinterpret_cast<TileAnimateTraitFn>(kTileAnimateTrait)(
			detailTile, kTileValue_user0,
			GetTileFloat(detailTile, kTileValue_user0), 1.0f,
			GetTileFloat(detailTile, kTileValue_user1));
	}

	static __declspec(naked) void HookStatsMenuDetails()
	{
		__asm
		{
			pushad
			mov eax, [esp + 40]
			push eax
			mov eax, [esp + 40]
			push eax
			push ecx
			call HandleStatsMenuDetails
			popad
			jmp dword ptr[g_statsMenuDetailsOriginal]
		}
	}

	bool InstallHooks()
	{
		g_appliedPatches = 0;
		g_failedPatches = 0;

		bool ok = true;

		ok &= WriteRelCallChained("ClassMenu commit sentinel-major hook",
			0x00597521,
			kClassMenuCommit,
			reinterpret_cast<UInt32>(&HookClassMenuCommit),
			g_classMenuCommitOriginalTarget);

		for (UInt32 i = 0; i < sizeof(kClassMenuRefreshDetailsCalls) / sizeof(kClassMenuRefreshDetailsCalls[0]); ++i)
			ok &= WriteRelCallChained("ClassMenu synthetic-major display hook",
				kClassMenuRefreshDetailsCalls[i],
				kClassMenuRefreshDetails,
				reinterpret_cast<UInt32>(&HookClassMenuRefreshDetails),
				g_classMenuRefreshDetailsOriginalTarget);

		ok &= InstallFunctionJumpHook("SkillsMenu_Open mode-capture entry hook",
			kSkillsMenuOpenEntry,
			kSkillsMenuOpenEntryExpected,
			sizeof(kSkillsMenuOpenEntryExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuOpenEntry),
			kSkillsMenuOpenEntryPatchLength,
			g_skillsMenuOpenEntryOriginal);

		{
			const UInt8* actual = reinterpret_cast<const UInt8*>(kSkillsMenuOpenXmlPathPush);
			if (actual[0] != 0x68)
			{
				_ERROR("TCS: SkillsMenu XML path push signature mismatch at %08X (expected opcode 0x68, got %02X)",
					kSkillsMenuOpenXmlPathPush, actual[0]);
				++g_failedPatches;
				ok = false;
			}
			else
			{
				ok &= WriteRelJumpChecked("SkillsMenu mode-0 XML path swap",
					kSkillsMenuOpenXmlPathPush,
					actual,
					1,
					reinterpret_cast<UInt32>(&HookSkillsMenuOpenXmlPathPush),
					5);
			}
		}

		ok &= WriteRelCallChained("SkillsMenu class-skill row injection hook",
			kSkillsMenuPreselectCall,
			kSkillsMenuPreselect,
			reinterpret_cast<UInt32>(&HookSkillsMenuPreselect),
			g_skillsMenuPreselectOriginalTarget);

		ok &= InstallFunctionJumpHook("SkillsMenu class-skill writeback hook",
			kSkillsMenuAccept,
			kSkillsMenuAcceptExpected,
			sizeof(kSkillsMenuAcceptExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuAccept),
			kSkillsMenuAcceptPatchLength,
			g_skillsMenuAcceptOriginal);

		ok &= InstallFunctionJumpHook("SkillsMenu synthetic-skill details hook",
			kSkillsMenuDetails,
			kSkillsMenuDetailsExpected,
			sizeof(kSkillsMenuDetailsExpected),
			reinterpret_cast<UInt32>(&HookSkillsMenuDetails),
			kSkillsMenuDetailsPatchLength,
			g_skillsMenuDetailsOriginal);

		ok &= WriteRelCallChained("StatsMenu skill row creation hook",
			kStatsMenuCreateRowsCall,
			kStatsMenuCreateRows,
			reinterpret_cast<UInt32>(&HookStatsMenuCreateRows),
			g_statsMenuCreateRowsOriginalTarget);

		for (UInt32 i = 0; i < sizeof(kStatsMenuRefreshCalls) / sizeof(kStatsMenuRefreshCalls[0]); ++i)
			ok &= WriteRelCallChained("StatsMenu refresh hook",
				kStatsMenuRefreshCalls[i],
				kStatsMenuRefresh,
				reinterpret_cast<UInt32>(&HookStatsMenuRefresh),
				g_statsMenuRefreshOriginalTarget);

		ok &= InstallFunctionJumpHook("StatsMenu detail pane hook",
			kStatsMenuDetails,
			kStatsMenuDetailsExpected,
			sizeof(kStatsMenuDetailsExpected),
			reinterpret_cast<UInt32>(&HookStatsMenuDetails),
			kStatsMenuDetailsPatchLength,
			g_statsMenuDetailsOriginal);

		g_originalTESDescriptionGetText = reinterpret_cast<void*>(
			DetourVtable(kTESDescriptionGetTextVtableSlot, reinterpret_cast<UInt32>(&HookTESDescriptionGetText)));
		_MESSAGE("TCS: detoured TESDescription::GetText vtable slot, original=%p", g_originalTESDescriptionGetText);

		ok &= InstallFunctionJumpHook("TESSkill_GetMasteryDescription hook",
			kTESSkillGetMasteryDescription,
			kTESSkillGetMasteryDescriptionExpected,
			sizeof(kTESSkillGetMasteryDescriptionExpected),
			reinterpret_cast<UInt32>(&HookTESSkillGetMasteryDescription),
			kTESSkillGetMasteryDescriptionPatchLength,
			g_originalTESSkillGetMasteryDescription);

		_MESSAGE("TCS: hooks installed applied=%u failed=%u", g_appliedPatches, g_failedPatches);
		return g_failedPatches == 0;
	}

}