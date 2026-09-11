#include "Defs.h"

namespace TCS
{

	SkillDefinition g_skills[kMaxCustomSkills] = {};

	UInt32 g_skillCount = 0;

	SkillState g_states[kMaxCustomSkills] = {};
	UInt8 g_postLoadPushCountdown[kMaxCustomSkills] = {};

	void NormalizeState(UInt32 index)
	{
		if (index >= kMaxCustomSkills)
			return;

		SkillState& state = g_states[index];
		if (state.level > kMaxSkillLevel)
			state.level = kMaxSkillLevel;
		if (!std::isfinite(state.progress) || state.progress < 0.0f)
			state.progress = 0.0f;
		if (!std::isfinite(state.requiredProgress) || state.requiredProgress <= 0.0f)
			state.requiredProgress = 1.0f;
		if (state.requiredProgress == 1.0f && state.level == 0 && state.progress == 0.0f && state.levelUps == 0)
			state.level = 5;
	}

	float GetProgressFraction(UInt32 index)
	{
		if (index >= kMaxCustomSkills)
			return 0.0f;

		const SkillState& state = g_states[index];
		if (state.level >= kMaxSkillLevel || state.requiredProgress <= 0.0f)
			return 0.0f;

		const float fraction = state.progress / state.requiredProgress;
		if (!std::isfinite(fraction) || fraction < 0.0f)
			return 0.0f;
		return fraction > 1.0f ? 1.0f : fraction;
	}

	static void EnsureDummySkillRegistered()
	{
		if (g_skillCount != 0)
			return;

		g_skills[0].skillId = 1;
		g_skills[0].editorId = "DummySkill";
		g_skills[0].name = "Dummy Skill";
		g_skills[0].governingAttributeAV = kActorVal_Strength;
		g_skills[0].specialization = 0;
		g_skills[0].xpCurve = XPCurveMode::kVanilla;
		g_skillCount = 1;
	}

	static UInt32 HashSkillEditorId(const std::string& editorId)
	{
		UInt32 hash = 2166136261u;
		for (char c : editorId)
			hash = (hash ^ static_cast<UInt8>(std::tolower(static_cast<unsigned char>(c)))) * 16777619u;
		hash &= 0x007FFFFFu;
		if (hash == 0)
			hash ^= 0x00A5A5A5u;
		return hash;
	}

	static UInt32 ResolveGoverningAttribute(const std::string& value)
	{
		static const std::pair<const char*, UInt32> kAttributes[] =
		{
			{"Strength", kActorVal_Strength},
			{"Intelligence", kActorVal_Intelligence},
			{"Willpower", kActorVal_Willpower},
			{"Agility", kActorVal_Agility},
			{"Speed", kActorVal_Speed},
			{"Endurance", kActorVal_Endurance},
			{"Personality", kActorVal_Personality},
			{"Luck", kActorVal_Luck},
		};
		for (const auto& entry : kAttributes)
		{
			if (!_stricmp(value.c_str(), entry.first))
				return entry.second;
		}
		_MESSAGE("TCS: unrecognized governingAttribute \"%s\", defaulting to Strength", value.c_str());
		return kActorVal_Strength;
	}

	static UInt32 ResolveSpecialization(const std::string& value)
	{
		if (!_stricmp(value.c_str(), "Magic"))
			return 1;
		if (!_stricmp(value.c_str(), "Stealth"))
			return 2;
		if (_stricmp(value.c_str(), "Combat"))
			_MESSAGE("TCS: unrecognized specialization \"%s\", defaulting to Combat", value.c_str());
		return 0;
	}

	static XPCurveMode ResolveXPCurve(const std::string& value)
	{
		if (!_stricmp(value.c_str(), "linear"))
			return XPCurveMode::kLinear;
		if (!_stricmp(value.c_str(), "custom"))
			return XPCurveMode::kCustom;
		if (_stricmp(value.c_str(), "vanilla"))
			_MESSAGE("TCS: unrecognized xpCurve \"%s\", defaulting to vanilla", value.c_str());
		return XPCurveMode::kVanilla;
	}

	static bool ParseRaceBonusKey(const std::string& key, std::string& outMod, UInt32& outObjectId)
	{
		const size_t sep = key.find('~');
		if (sep == std::string::npos || sep == 0 || sep + 1 >= key.size())
			return false;

		std::string idPart = key.substr(0, sep);
		outMod = key.substr(sep + 1);

		if (idPart.size() > 1 && idPart[0] == '0' && (idPart[1] == 'x' || idPart[1] == 'X'))
			idPart = idPart.substr(2);
		if (idPart.empty())
			return false;

		char* end = nullptr;
		const unsigned long parsed = std::strtoul(idPart.c_str(), &end, 16);
		if (!end || *end)
			return false;

		outObjectId = static_cast<UInt32>(parsed) & 0x00FFFFFF;
		return true;
	}

	static bool ResolveNativeSkillActorValue(const std::string& name, UInt32& outAV)
	{
		static const std::pair<const char*, UInt32> kNativeSkills[] =
		{
			{ "Blade", kActorVal_Blade },
			{ "Blunt", kActorVal_Blunt },
			{ "Hand to Hand", kActorVal_HandToHand },
			{ "Armorer", kActorVal_Armorer },
			{ "Heavy Armor", kActorVal_HeavyArmor },
			{ "Light Armor", kActorVal_LightArmor },
			{ "Block", kActorVal_Block },
			{ "Athletics", kActorVal_Athletics },
			{ "Acrobatics", kActorVal_Acrobatics },
			{ "Marksman", kActorVal_Marksman },
			{ "Security", kActorVal_Security },
			{ "Sneak", kActorVal_Sneak },
			{ "Mercantile", kActorVal_Mercantile },
			{ "Speechcraft", kActorVal_Speechcraft },
			{ "Alchemy", kActorVal_Alchemy },
			{ "Alteration", kActorVal_Alteration },
			{ "Conjuration", kActorVal_Conjuration },
			{ "Destruction", kActorVal_Destruction },
			{ "Illusion", kActorVal_Illusion },
			{ "Mysticism", kActorVal_Mysticism },
			{ "Restoration", kActorVal_Restoration },
		};

		for (const auto& entry : kNativeSkills)
		{
			if (!_stricmp(entry.first, name.c_str()))
			{
				outAV = entry.second;
				return true;
			}
		}
		return false;
	}

	static bool ResolveMajorSkillEntry(const std::string& name, UInt32& outAV)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!_stricmp(g_skills[i].editorId.c_str(), name.c_str()) || !_stricmp(g_skills[i].name.c_str(), name.c_str()))
			{
				if (g_skills[i].realActorValue == 0)
				{
					_MESSAGE("TCS: class override references skill \"%s\" which has no registered actor value yet -- skipping this slot", name.c_str());
					return false;
				}
				outAV = g_skills[i].realActorValue;
				return true;
			}
		}

		return ResolveNativeSkillActorValue(name, outAV);
	}

	static bool ParseClassJSON(const std::string& filePath, ClassMajorOverride& outDef)
	{
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			_MESSAGE("TCS: failed to open class file %s", filePath.c_str());
			return false;
		}

		nlohmann::json j;
		try
		{
			file >> j;
		}
		catch (const std::exception& e)
		{
			_MESSAGE("TCS: failed to parse class file %s: %s", filePath.c_str(), e.what());
			return false;
		}

		if (!j.contains("editorId") || !j["editorId"].is_string() || j["editorId"].get<std::string>().empty())
		{
			_MESSAGE("TCS: class file %s missing required non-empty \"editorId\" string", filePath.c_str());
			return false;
		}

		outDef.editorId = j["editorId"].get<std::string>();
		outDef.name = j.value("name", outDef.editorId);
		outDef.description = j.value("description", std::string(""));
		outDef.iconPath = j.value("icon", std::string(""));

		if (!j.contains("majorSkills") || !j["majorSkills"].is_array() || j["majorSkills"].size() != 7)
		{
			_MESSAGE("TCS: class file %s \"majorSkills\" must be an array of exactly 7 skill names", filePath.c_str());
			return false;
		}

		for (UInt32 i = 0; i < 7; ++i)
		{
			if (!j["majorSkills"][i].is_string())
			{
				_MESSAGE("TCS: class file %s majorSkills[%u] is not a string", filePath.c_str(), i);
				return false;
			}
			outDef.majorSkillNames[i] = j["majorSkills"][i].get<std::string>();
		}

		return true;
	}

	static TESClass* CreateSyntheticClass(const ClassMajorOverride& def, const UInt32 resolvedMajors[7], UInt32 forcedFormId)
	{
		void* mem = FormHeap_Allocate(sizeof(TESClass));
		if (!mem)
		{
			_MESSAGE("TCS: FormHeap_Allocate failed for synthetic class \"%s\"", def.editorId.c_str());
			return nullptr;
		}

		TESClass* tesClass = reinterpret_cast<TESClassCtorFn>(kTESClassCtor)(reinterpret_cast<TESClass*>(mem));

		if (forcedFormId)
			tesClass->refID = forcedFormId; // set BEFORE anything below can register/insert this object under the wrong ID

		tesClass->fullName.name.Set(def.name.c_str());
		tesClass->texture.ddsPath.Set(def.iconPath.c_str());

		for (UInt32 i = 0; i < 7; ++i)
			tesClass->majorSkills[i] = resolvedMajors[i];

		tesClass->classFlags |= TESClass::kFlag_Playable;

		if (g_classOverrideCount < kMaxCustomClasses)
		{
			g_classOverrides[g_classOverrideCount].editorId = def.editorId;
			g_classOverrides[g_classOverrideCount].tesClass = tesClass;
			g_classOverrides[g_classOverrideCount].description = def.description;
			g_classOverrides[g_classOverrideCount].isSynthetic = true;
			++g_classOverrideCount;
		}

		UInt8* dataHandlerBase = reinterpret_cast<UInt8*>(*g_dataHandler);
		reinterpret_cast<BSSimpleListInsertSortedFn>(kBSSimpleListInsertSorted)(
			dataHandlerBase + kTESDataHandlerClassListOffset, tesClass, kClassSortComparator);

		_MESSAGE("TCS: created synthetic class \"%s\" formID=%08X", def.editorId.c_str(), tesClass->refID);
		return tesClass;
	}

	static TESClass* CreateSyntheticClass(const ClassMajorOverride& def, const UInt32 resolvedMajors[7])
	{
		return CreateSyntheticClass(def, resolvedMajors, 0);
	}

	static TESClass* FindOrCreateSyntheticClass(const ClassMajorOverride& def, const UInt32 resolvedMajors[7])
	{
		for (UInt32 i = 0; i < g_classOverrideCount; ++i)
		{
			if (g_classOverrides[i].editorId == def.editorId && g_classOverrides[i].tesClass)
			{
				TESClass* tesClass = g_classOverrides[i].tesClass;
				_MESSAGE("TCS: reused already-live synthetic class \"%s\" formID=%08X (created earlier this session)",
					def.editorId.c_str(), tesClass->refID);

				tesClass->fullName.name.Set(def.name.c_str());
				for (UInt32 s = 0; s < 7; ++s)
					tesClass->majorSkills[s] = resolvedMajors[s];

				return tesClass;
			}
		}

		const UInt32 editorIdHash = HashSkillEditorId(def.editorId);
		for (const auto& saved : g_savedSyntheticClasses)
		{
			if (saved.editorIdHash != editorIdHash)
				continue;

			return CreateSyntheticClass(def, resolvedMajors, saved.lastKnownFormId);
		}

		return CreateSyntheticClass(def, resolvedMajors);
	}

	static bool ParseSkillJSON(const std::string& filePath, SkillDefinition& outDef)
	{
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			_MESSAGE("TCS: failed to open skill file %s", filePath.c_str());
			return false;
		}

		nlohmann::json j;
		try
		{
			file >> j;
		}
		catch (const std::exception& e)
		{
			_MESSAGE("TCS: failed to parse skill file %s: %s", filePath.c_str(), e.what());
			return false;
		}

		if (!j.contains("editorId") || !j["editorId"].is_string() || j["editorId"].get<std::string>().empty())
		{
			_MESSAGE("TCS: skill file %s missing required non-empty \"editorId\" string", filePath.c_str());
			return false;
		}

		const std::string editorId = j["editorId"].get<std::string>();
		outDef.skillId = HashSkillEditorId(editorId);
		outDef.editorId = editorId;
		outDef.name = j.value("name", editorId);
		outDef.description = j.value("description", std::string(""));
		outDef.iconLarge = j.value("icon", std::string(""));
		outDef.iconSmall = j.value("iconSmall", std::string(""));
		outDef.governingAttributeAV = ResolveGoverningAttribute(j.value("governingAttribute", std::string("Strength")));
		outDef.specialization = ResolveSpecialization(j.value("specialization", std::string("Combat")));
		outDef.xpCurve = ResolveXPCurve(j.value("xpCurve", std::string("vanilla")));
		outDef.apprenticeText = j.value("apprenticeText", std::string(""));
		outDef.journeymanText = j.value("journeymanText", std::string(""));
		outDef.expertText = j.value("expertText", std::string(""));
		outDef.masterText = j.value("masterText", std::string(""));
		outDef.raceBonuses.clear();
		if (j.contains("raceBonuses") && j["raceBonuses"].is_object())
		{
			for (auto it = j["raceBonuses"].begin(); it != j["raceBonuses"].end(); ++it)
			{
				if (!it.value().is_number_integer() && !it.value().is_number_unsigned())
				{
					_MESSAGE("TCS: skill \"%s\" raceBonuses[\"%s\"] is not an integer, skipping",
						editorId.c_str(), it.key().c_str());
					continue;
				}

				std::string sourceMod;
				UInt32 objectId = 0;
				if (!ParseRaceBonusKey(it.key(), sourceMod, objectId))
				{
					_MESSAGE("TCS: skill \"%s\" raceBonuses key \"%s\" is malformed (expected \"0xFFFFFF~ModName.esp\"), skipping",
						editorId.c_str(), it.key().c_str());
					continue;
				}

				const SInt32 rawValue = it.value().get<SInt32>();
				if (rawValue < 0)
				{
					_MESSAGE("TCS: skill \"%s\" raceBonuses[\"%s\"] is negative, skipping",
						editorId.c_str(), it.key().c_str());
					continue;
				}

				outDef.raceBonuses.push_back({ sourceMod, objectId, static_cast<UInt32>(rawValue) });
			}
		}
		return true;
	}

	static bool ApplyClassMajorOverride(const ClassMajorOverride& def, const std::string& filePath)
	{
		const UInt32 formId = EditorIDMapper::Lookup(def.editorId.c_str());
		if (!formId)
		{
			_MESSAGE("TCS: class override \"%s\" could not be resolved (EditorIDMapper not ready or unknown editorId) from %s", def.editorId.c_str(), filePath.c_str());
			return false;
		}

		TESForm* form = LookupFormByID(formId);
		if (!form || form->typeID != kFormType_Class)
		{
			_MESSAGE("TCS: class override \"%s\" resolved formId=%08X is not a TESClass", def.editorId.c_str(), formId);
			return false;
		}

		TESClass* tesClass = reinterpret_cast<TESClass*>(form);

		UInt32 resolved[7] = {};
		for (UInt32 i = 0; i < 7; ++i)
		{
			if (!ResolveMajorSkillEntry(def.majorSkillNames[i], resolved[i]))
			{
				_MESSAGE("TCS: class override \"%s\" majorSkills[%u]=\"%s\" could not be resolved -- aborting this file, no partial overwrite",
					def.editorId.c_str(), i, def.majorSkillNames[i].c_str());
				return false;
			}
		}

		for (UInt32 i = 0; i < 7; ++i)
			tesClass->majorSkills[i] = resolved[i];

		_MESSAGE("TCS: class \"%s\" majorSkills overwritten from %s", def.editorId.c_str(), filePath.c_str());
		return true;
	}

	static UInt32 FindExistingActorValueByName(const std::string& skillName)
	{
		if (!ResolveAddActorValues() || !g_avTokenLookupNext)
			return 0;

		void* token = reinterpret_cast<AVTokenLookupNextFn>(g_avTokenLookupNext)(nullptr);
		while (token)
		{
			TESObjectMISC* misc = reinterpret_cast<TESObjectMISC*>(token);
			const char* existingName = misc->fullName.name.m_data;
			if (existingName && _stricmp(existingName, skillName.c_str()) == 0)
			{
				const UInt32 existingAV = reinterpret_cast<TESForm*>(token)->refID;
				_MESSAGE("TCS: found existing AV \"%s\" (avCode=%08X) — reusing instead of registering a duplicate", existingName, existingAV);
				return existingAV;
			}
			token = reinterpret_cast<AVTokenLookupNextFn>(g_avTokenLookupNext)(token);
		}
		return 0;
	}

	UInt32 RegisterCustomActorValue(const std::string& skillName, bool& outWasReused)
	{
		outWasReused = false;
		if (!ResolveAddActorValues())
			return 0;

		if (const UInt32 existing = FindExistingActorValueByName(skillName))
		{
			outWasReused = true;
			return existing;
		}

		TESForm* form = ::CreateFormInstance(kFormType_Misc);
		if (!form)
		{
			_MESSAGE("TCS: CreateFormInstance failed for skill \"%s\"", skillName.c_str());
			return 0;
		}

		TESObjectMISC* misc = reinterpret_cast<TESObjectMISC*>(form);
		misc->value.value = static_cast<SInt32>(form->refID);
		misc->weight.weight = static_cast<float>(kAVGroup_Skill);
		misc->fullName.name.Set(skillName.c_str());
		form->flags |= (TESForm::kFormFlags_BorderRegion | TESForm::kFormFlags_TurnOffFire);

		const bool registered = reinterpret_cast<AVTokenRegisterFn>(g_avTokenRegister)(misc);
		if (!registered)
		{
			_MESSAGE("TCS: AVToken::Register rejected skill \"%s\" (refID=%08X)", skillName.c_str(), form->refID);
			return 0;
		}

		_MESSAGE("TCS: registered genuine AV for skill \"%s\", avCode=%08X", skillName.c_str(), form->refID);
		return form->refID;
	}

	static PlayerCharacter* GetPlayer()
	{
		return g_thePlayer ? *g_thePlayer : nullptr;
	}

	UInt32 GetRealAVLevel(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return 0;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return 0;
		return static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);
	}

	void PushSkillLevelToRealAV(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;
		const UInt32 currentAVLevel = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);
		const SInt32 delta = static_cast<SInt32>(g_states[index].level) - static_cast<SInt32>(currentAVLevel);
		if (delta != 0)
			player->ModBaseAV(g_skills[index].realActorValue, delta);
	}

	void ForceSetSkillLevelOnRealAV(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		const UInt32 beforeValue = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);
		player->SetAV_F(g_skills[index].realActorValue, static_cast<float>(g_states[index].level));
		const UInt32 afterValue = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);
		_MESSAGE("TCS: ForceSetSkillLevelOnRealAV skillId=%u beforeValue=%u setTo=%u afterValue=%u",
			g_skills[index].skillId, beforeValue, g_states[index].level, afterValue);
	}

	static void ContributeMajorSkillAdvances(UInt32 index, UInt32 levelUps);

	static void ContributeAttributeBonusBucket(UInt32 index, UInt32 levelUps)
	{
		if (!levelUps || index >= g_skillCount || g_skills[index].governingAttributeAV > kActorVal_Luck)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;
		for (UInt32 i = 0; i < levelUps; ++i)
			reinterpret_cast<PlayerIncrementAttributeBonusBucketFn>(kPlayerIncrementAttributeBonusBucket)(player, g_skills[index].governingAttributeAV);
	}

	void ReconcileSkillLevelWithRealAV(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
		{
			return;
		}
		PlayerCharacter* player = GetPlayer();
		if (!player)
		{
			return;
		}

		NormalizeState(index);
		SkillState& state = g_states[index];
		const UInt32 avLevel = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);

		if (avLevel > state.level)
		{
			const UInt32 previousLevel = state.level;
			state.level = (avLevel > kMaxSkillLevel) ? kMaxSkillLevel : avLevel;
			state.progress = 0.0f;
			state.levelUps += (state.level - previousLevel);
			state.governingAttributeIncreaseCount += (state.level - previousLevel);
			ContributeMajorSkillAdvances(index, state.level - previousLevel);
			ContributeAttributeBonusBucket(index, state.level - previousLevel);
		}
	}

	void ReconcileSkillProgressWithXSkills(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return;

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired))
			return;

		if (!std::isfinite(xProgress) || xProgress < 0.0f)
			return;
		if (!std::isfinite(xRequired) || xRequired <= 0.0f)
			return;

		SkillState& state = g_states[index];
		if (state.progress != xProgress || state.requiredProgress != xRequired)
		{
			_MESSAGE("TCS: ReconcileSkillProgressWithXSkills skillId=%u adopted progress=%.2f/%.2f (was %.2f/%.2f)",
				g_skills[index].skillId, xProgress, xRequired, state.progress, state.requiredProgress);
			state.progress = xProgress;
			state.requiredProgress = xRequired;
		}
	}

	static UInt32 GetPlayerClassSpecialization()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return 0xFFFFFFFF;

		TESClass* playerClass = player->GetPlayerClass();
		if (!playerClass)
			return 0xFFFFFFFF;

		return playerClass->specialization;
	}

	static float g_lastCorrectedRequiredProgress[kMaxCustomSkills] = {};
	static float g_lastLoggedRawRequiredProgress[kMaxCustomSkills] = {};

	void ApplyMajorSpecializationScaling(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return;

		const bool isMajor = g_states[index].major != 0;
		if (!isMajor)
			return;

		constexpr float kMajorMultiplier = 0.6f;

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired))
			return;
		if (!std::isfinite(xRequired) || xRequired <= 0.0f)
			return;

		if (xRequired == g_lastCorrectedRequiredProgress[index])
			return;

		const float correctedRequired = xRequired * kMajorMultiplier;
		if (!std::isfinite(correctedRequired) || correctedRequired <= 0.0f)
			return;

		if (WriteXSkillsProgress(g_skills[index].realActorValue, xProgress, correctedRequired))
		{
			g_lastCorrectedRequiredProgress[index] = correctedRequired;
		}
	}

	static bool ResolveRaceBonusFormId(const SkillRaceBonus& entry, UInt32& outFormId)
	{
		outFormId = 0;
		if (!g_dataHandler || !*g_dataHandler)
			return false;

		const UInt8 modIndex = (*g_dataHandler)->GetModIndex(entry.sourceMod.c_str());
		if (modIndex == 0xFF)
			return false;

		outFormId = (static_cast<UInt32>(modIndex) << 24) | (entry.objectId & 0x00FFFFFF);
		return true;
	}

	static UInt32 GetRaceBonusForSkill(UInt32 index, UInt32 raceFormId)
	{
		if (index >= g_skillCount || !raceFormId)
			return 0;

		for (const SkillRaceBonus& entry : g_skills[index].raceBonuses)
		{
			UInt32 resolvedFormId = 0;
			if (ResolveRaceBonusFormId(entry, resolvedFormId) && resolvedFormId == raceFormId)
				return entry.bonus;
		}
		return 0;
	}

	void ApplyRaceBonusesAtCharacterCreation(UInt32 raceFormId)
	{
		if (!raceFormId)
			return;

		EnsureCustomActorValuesRegistered();

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!g_skills[i].isOwnForm)
				continue;

			const UInt32 bonus = GetRaceBonusForSkill(i, raceFormId);
			if (!bonus)
				continue;

			NormalizeState(i);
			SkillState& state = g_states[i];
			const UInt32 previousLevel = state.level;
			const UInt32 newLevel = state.level + bonus;
			state.level = (newLevel > kMaxSkillLevel) ? kMaxSkillLevel : newLevel;

			_MESSAGE("TCS: race bonus applied skillId=%u raceFormId=%08X bonus=%u level %u -> %u",
				g_skills[i].skillId, raceFormId, bonus, previousLevel, state.level);

			if (g_skills[i].isOwnForm && g_skills[i].realActorValue != 0)
				PushSkillLevelToRealAV(i);
		}
	}


	void ApplyClassSpecializationBonusAtCharacterCreation()
	{
		PlayerCharacter* player = *g_thePlayer;
		if (!player || !player->baseForm)
			return;

		TESNPC* npc = reinterpret_cast<TESNPC*>(player->baseForm);
		TESClass* npcClass = npc->npcClass;
		if (!npcClass)
			return;

		EnsureCustomActorValuesRegistered();

		const UInt32 classSpecialization = npcClass->specialization;

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].specialization != classSpecialization || !g_skills[i].isOwnForm)
				continue;

			NormalizeState(i);
			SkillState& state = g_states[i];
			const UInt32 previousLevel = state.level;
			const UInt32 newLevel = state.level + 5;
			state.level = (newLevel > kMaxSkillLevel) ? kMaxSkillLevel : newLevel;

			_MESSAGE("TCS: specialization bonus applied skillId=%u classSpecialization=%u level %u -> %u",
				g_skills[i].skillId, classSpecialization, previousLevel, state.level);

			if (g_skills[i].isOwnForm && g_skills[i].realActorValue != 0)
				PushSkillLevelToRealAV(i);
		}
	}

	void ApplyPremadeClassMajorsAtCharacterCreation()
	{
		PlayerCharacter* player = GetPlayer();
		if (!player || !player->baseForm)
			return;

		TESNPC* npc = reinterpret_cast<TESNPC*>(player->baseForm);
		TESClass* npcClass = npc->npcClass;
		if (!npcClass)
			return;

		for (UInt32 slot = 0; slot < 7; ++slot)
		{
			const UInt32 majorAV = npcClass->majorSkills[slot];

			for (UInt32 i = 0; i < g_skillCount; ++i)
			{
				if (g_skills[i].realActorValue != majorAV)
					continue;

				if (g_states[i].major)
					break;

				g_states[i].major = 1;
				NormalizeState(i);
				const UInt32 realLevel = GetRealAVLevel(i);
				UInt32 base = g_states[i].level;
				if (realLevel > base)
					base = realLevel;
				UInt32 target = base + 20;
				if (target > kMaxSkillLevel)
					target = kMaxSkillLevel;

				_MESSAGE("TCS: premade class major applied skillId=%u slot=%u level %u -> %u",
					g_skills[i].skillId, slot, g_states[i].level, target);

				g_states[i].level = target;

				if (g_skills[i].realActorValue != 0)
					PushSkillLevelToRealAV(i);

				break;
			}
		}
	}

	bool EnsureCustomActorValuesRegistered()
	{
		g_customActorValuesEverRegistered = true;

		bool linkedAny = false;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].realActorValue != 0)
				continue;

			bool wasReused = false;
			g_skills[i].realActorValue = RegisterCustomActorValue(g_skills[i].name, wasReused);

			if (g_skills[i].realActorValue == 0)
				continue;

			linkedAny = true;

			if (wasReused)
			{
				PlayerCharacter* player = GetPlayer();
				if (player)
				{
					NormalizeState(i);
					UInt32 avLevel = static_cast<UInt32>(player->GetAV_F(g_skills[i].realActorValue) + 0.5f);
					if (avLevel > kMaxSkillLevel)
						avLevel = kMaxSkillLevel;
					if (avLevel > g_states[i].level)
						g_states[i].level = avLevel;
				}
			}
			else
			{
				NormalizeState(i);
				PushSkillLevelToRealAV(i);
			}

			const bool linked = LinkSkillWithXSkills(g_skills[i].realActorValue, g_skills[i].name.c_str());
			g_skills[i].isOwnForm = linked;

			if (linked)
			{
				const bool wroteAttribute = SetXSkillsGoverningAttributeAndSpecialization(g_skills[i].realActorValue,
					g_skills[i].governingAttributeAV, g_skills[i].specialization);
				_MESSAGE("TCS: SetXSkillsGoverningAttributeAndSpecialization for \"%s\" avCode=%08X governingAttributeAV=%u specialization=%u -> %s",
					g_skills[i].name.c_str(), g_skills[i].realActorValue, g_skills[i].governingAttributeAV, g_skills[i].specialization,
					wroteAttribute ? "OK" : "FAILED");

				const bool wroteIcon = SetXSkillsIcon(g_skills[i].realActorValue, g_skills[i].iconLarge);
				_MESSAGE("TCS: SetXSkillsIcon for \"%s\" avCode=%08X iconLarge=\"%s\" -> %s",
					g_skills[i].name.c_str(), g_skills[i].realActorValue, g_skills[i].iconLarge.c_str(),
					wroteIcon ? "OK" : "FAILED");

				g_skills[i].xSkillsForm = GetXSkillsFormForAV(g_skills[i].realActorValue);
				_MESSAGE("TCS: captured xSkillsForm=%p for \"%s\"", g_skills[i].xSkillsForm, g_skills[i].name.c_str());
			}
		}

		return linkedAny;
	}

	void LoadSkillDefinitionsFromDisk()
	{
		g_skillCount = 0;

		std::string searchPattern = kSkillsDirectory;
		searchPattern += "*.json";

		WIN32_FIND_DATAA findData = {};
		HANDLE findHandle = FindFirstFileA(searchPattern.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			_MESSAGE("TCS: no skill files found in %s (missing folder, or genuinely empty)", kSkillsDirectory);
			return;
		}

		UInt32 attempted = 0;
		UInt32 skipped = 0;
		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			++attempted;
			if (g_skillCount >= kMaxCustomSkills)
			{
				_MESSAGE("TCS: kMaxCustomSkills (%u) reached, ignoring remaining skill files", kMaxCustomSkills);
				break;
			}

			std::string filePath = kSkillsDirectory;
			filePath += findData.cFileName;

			SkillDefinition parsed = {};
			if (!ParseSkillJSON(filePath, parsed))
			{
				++skipped;
				continue;
			}

			bool duplicate = false;
			for (UInt32 i = 0; i < g_skillCount; ++i)
			{
				if (!_stricmp(g_skills[i].editorId.c_str(), parsed.editorId.c_str()))
				{
					_MESSAGE("TCS: duplicate editorId \"%s\" in %s — skipping, first definition wins", parsed.editorId.c_str(), filePath.c_str());
					duplicate = true;
					break;
				}
			}
			if (duplicate)
			{
				++skipped;
				continue;
			}

			g_skills[g_skillCount++] = parsed;
			_MESSAGE("TCS: loaded skill \"%s\" (id=%u) icon=\"%s\" iconSmall=\"%s\" from %s",
				parsed.name.c_str(), parsed.skillId, parsed.iconLarge.c_str(), parsed.iconSmall.c_str(), filePath.c_str());
		} while (FindNextFileA(findHandle, &findData));
		FindClose(findHandle);

		_MESSAGE("TCS: skill registry loaded %u/%u files (%u skipped)", g_skillCount, attempted, skipped);
	}

	void LoadClassDefinitionsFromDisk()
	{
		EnsureCustomActorValuesRegistered();

		std::string searchPattern = kClassesDirectory;
		searchPattern += "*.json";

		WIN32_FIND_DATAA findData = {};
		HANDLE findHandle = FindFirstFileA(searchPattern.c_str(), &findData);
		if (findHandle == INVALID_HANDLE_VALUE)
		{
			_MESSAGE("TCS: no class override files found in %s", kClassesDirectory);
			return;
		}

		UInt32 attempted = 0;
		UInt32 applied = 0;
		do
		{
			if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				continue;

			++attempted;
			std::string filePath = kClassesDirectory;
			filePath += findData.cFileName;

			ClassMajorOverride parsed = {};
			if (!ParseClassJSON(filePath, parsed))
				continue;

			const UInt32 formId = EditorIDMapper::Lookup(parsed.editorId.c_str());
			if (formId)
			{
				if (ApplyClassMajorOverride(parsed, filePath))
					++applied;
			}
			else
			{
				UInt32 resolved[7] = {};
				bool allResolved = true;
				for (UInt32 i = 0; i < 7; ++i)
				{
					if (!ResolveMajorSkillEntry(parsed.majorSkillNames[i], resolved[i]))
					{
						_MESSAGE("TCS: new class \"%s\" majorSkills[%u]=\"%s\" could not be resolved -- aborting this file",
							parsed.editorId.c_str(), i, parsed.majorSkillNames[i].c_str());
						allResolved = false;
						break;
					}
				}

				if (allResolved)
				{
					TESClass* created = FindOrCreateSyntheticClass(parsed, resolved);
					if (created)
						++applied;
				}
			}
		} while (FindNextFileA(findHandle, &findData));
		FindClose(findHandle);

		if (g_lastKnownPlayerSyntheticClassFormId)
		{
			for (UInt32 i = 0; i < g_classOverrideCount; ++i)
			{
				if (g_classOverrides[i].tesClass && g_classOverrides[i].tesClass->refID == g_lastKnownPlayerSyntheticClassFormId)
				{
					if (PlayerCharacter* player = GetPlayer())
					{
						if (player->baseForm)
						{
							TESNPC* npc = reinterpret_cast<TESNPC*>(player->baseForm);
							_MESSAGE("TCS: repointing npc->npcClass from refID=%08X to recreated synthetic class refID=%08X",
								npc->npcClass ? npc->npcClass->refID : 0, g_classOverrides[i].tesClass->refID);
							npc->npcClass = g_classOverrides[i].tesClass;
						}
					}
					break;
				}
			}
		}

		_MESSAGE("TCS: class override registry processed %u/%u files (%u applied)", applied, attempted);
	}

	UInt32 GetSkillIndexById(UInt32 skillId)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].skillId == skillId)
				return i;
		}
		return 0xFFFFFFFF;
	}

	static UInt32 GetSkillIndexByEditorId(const char* editorId)
	{
		if (!editorId)
			return 0xFFFFFFFF;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (!_stricmp(g_skills[i].editorId.c_str(), editorId))
				return i;
		}
		return 0xFFFFFFFF;
	}

	const char* GetSkillIconLarge(UInt32 index)
	{
		if (index >= g_skillCount)
			return "";
		const std::string& path = g_skills[index].iconLarge;
		return path.c_str();
	}

	const char* GetSkillIconSmall(UInt32 index)
	{
		if (index >= g_skillCount)
			return "";
		const std::string& smallPath = g_skills[index].iconSmall;
		return smallPath.c_str();
	}

	static const char* GetSafeActorValueName(UInt32 av)
	{
		const char* name = reinterpret_cast<ActorValueGetNameFn>(kActorValueGetName)(av);
		return name ? name : "";
	}

	static const char* GetSafeMasteryName(UInt32 level)
	{
		switch (GetSkillMasteryLevel(level))
		{
		case kMasteryLevel_Novice: return "Novice";
		case kMasteryLevel_Apprentice: return "Apprentice";
		case kMasteryLevel_Journeyman: return "Journeyman";
		case kMasteryLevel_Expert: return "Expert";
		case kMasteryLevel_Master: return "Master";
		default: return "";
		}
	}

	void ComposeSkillDescriptionText(UInt32 index, char* buffer, UInt32 bufferSize)
	{
		if (!buffer || !bufferSize)
			return;
		if (index >= g_skillCount)
		{
			buffer[0] = '\0';
			return;
		}
		NormalizeState(index);
		const SkillState& state = g_states[index];
		_snprintf_s(buffer, bufferSize, _TRUNCATE,
			"%s\n\nGoverning Attribute: %s\n\nLevel: %s",
			g_skills[index].description.c_str(),
			GetSafeActorValueName(g_skills[index].governingAttributeAV),
			GetSafeMasteryName(state.level));
	}

	static float ComputeXPGain(UInt32 index, float rawAmount, float attributeValue, bool specializationMatches)
	{
		if (index >= g_skillCount)
			return 0.0f;

		const SkillDefinition& def = g_skills[index];
		float gain = rawAmount;

		if (def.xpCurve == XPCurveMode::kVanilla)
		{
			const float level = static_cast<float>(g_states[index].level);
			const float curveExponent = 1.2f;
			const float diminishing = std::pow(1.0f - (level / 100.0f), curveExponent);
			gain *= diminishing > 0.0f ? diminishing : 0.01f;
		}
		else if (def.xpCurve == XPCurveMode::kLinear)
		{
		}

		const float attributeScale = 1.0f + (attributeValue - 50.0f) * 0.005f;
		gain *= attributeScale > 0.0f ? attributeScale : 0.0f;

		if (specializationMatches)
			gain *= 1.1f;

		return gain;
	}

	static float RequiredProgressForLevel(UInt32 index, UInt32 level)
	{
		(void)index;
		(void)level;
		return 1.0f;
	}

	static bool IsEffectiveMajor(UInt32 index)
	{
		return index < g_skillCount && g_states[index].major != 0;
	}

	static UInt32 GetLevelUpSkillCount()
	{
		SettingInfo* setting = nullptr;
		if (GetGameSetting(const_cast<char*>("iLevelUpSkillCount"), &setting) && setting && setting->i > 0)
			return static_cast<UInt32>(setting->i);
		return 10;
	}

	static void ContributeMajorSkillAdvances(UInt32 index, UInt32 levelUps)
	{
		if (!levelUps || !IsEffectiveMajor(index))
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		const UInt32 levelUpSkillCount = GetLevelUpSkillCount();
		for (UInt32 i = 0; i < levelUps; ++i)
		{
			++player->majorSkillAdvances;
			if (levelUpSkillCount)
				reinterpret_cast<PlayerMaybeStartNextAttributeBonusBucketFn>(kPlayerMaybeStartNextAttributeBonusBucket)(player);
			if (levelUpSkillCount && player->majorSkillAdvances >= levelUpSkillCount)
				player->bCanLevelUp = 1;
		}
	}

	static void MirrorLevelUpSideEffects(UInt32 index, UInt32 levelUps)
	{
		if (!levelUps)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		ContributeAttributeBonusBucket(index, levelUps);
		ContributeMajorSkillAdvances(index, levelUps);
	}

	static bool ShowSkillPerkPopup(UInt32 index, UInt32 mastery)
	{
		if (index >= g_skillCount || mastery == 0)
			return false;

		const char* icon = GetSkillIconLarge(index);

		const char* description = nullptr;
		switch (mastery)
		{
		case 1: description = g_skills[index].apprenticeText.c_str(); break;
		case 2: description = g_skills[index].journeymanText.c_str(); break;
		case 3: description = g_skills[index].expertText.c_str(); break;
		case 4: description = g_skills[index].masterText.c_str(); break;
		default: return false;
		}

		if (!description || !description[0])
			return false;

		return reinterpret_cast<_OpenSkillPerkMenu>(kOpenSkillPerkMenu)(kSkillPerkMenuXml,
			0,
			1,
			0,
			kGenericMenuArgString,
			icon,
			kGenericMenuArgString,
			description,
			kGenericMenuArgString,
			kSkillPerkOkText,
			kGenericMenuArgEnd) != 0;
	}

	auto PlaySoundFn = reinterpret_cast<PlaySound_t>(0x006ADE50);

	static void NotifyLevelIncrease(UInt32 index, UInt32 previousLevel, UInt32 levelUps)
	{
		if (!levelUps || index >= g_skillCount)
			return;

		char message[256] = {};
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Your %s skill increased to %u.",
			g_skills[index].name.c_str(), g_states[index].level);
		::QueueUIMessage(message, 0, 1, 2.0f);

		OSSoundGlobals* soundGlobals = (*g_osGlobals)->sound;
		void* sound = PlaySoundFn(soundGlobals, "UIStatsSkillUp", 0x121, 1);
		if (sound)
		{
			ThisStdCall(0x6B7190, sound, 0);
			ThisStdCall(0x6B73E0, sound, 0);
			FormHeap_Free(sound);
		}

		const UInt32 previousMastery = GetSkillMasteryLevel(previousLevel);
		const UInt32 newMastery = GetSkillMasteryLevel(g_states[index].level);
		if (newMastery > previousMastery)
		{
			_snprintf_s(message, sizeof(message), _TRUNCATE, "You are now a %s in %s.",
				GetSafeMasteryName(g_states[index].level), g_skills[index].name.c_str());
			::QueueUIMessage(message, 0, 1, 4.0f);
			ShowSkillPerkPopup(index, newMastery);
		}
	}

	static bool AddSkillXPRaw(UInt32 skillId, float amount)
	{
		const UInt32 index = GetSkillIndexById(skillId);
		if (index >= g_skillCount || !std::isfinite(amount))
			return false;

		NormalizeState(index);
		SkillState& state = g_states[index];
		if (state.level >= kMaxSkillLevel)
			return true;

		UInt32 levelUps = 0;
		state.progress += amount;
		if (!std::isfinite(state.progress) || state.progress < 0.0f)
			state.progress = 0.0f;

		while (state.level < kMaxSkillLevel && state.progress + kProgressEpsilon >= state.requiredProgress)
		{
			state.progress -= state.requiredProgress;
			++state.level;
			++state.levelUps;
			++state.governingAttributeIncreaseCount;
			++levelUps;
			state.requiredProgress = RequiredProgressForLevel(index, state.level);
		}

		if (state.level >= kMaxSkillLevel)
			state.progress = 0.0f;

		if (levelUps)
		{
			MirrorLevelUpSideEffects(index, levelUps);
			PushSkillLevelToRealAV(index);
		}

		return true;
	}

	static bool AddSkillXP(UInt32 skillId, float rawAmount, float governingAttributeValue, bool specializationMatches)
	{
		const UInt32 index = GetSkillIndexById(skillId);
		if (index >= g_skillCount)
			return false;

		const float gain = ComputeXPGain(index, rawAmount, governingAttributeValue, specializationMatches);
		return AddSkillXPRaw(skillId, gain);
	}

	static void SaveCallback(void*)
	{
		if (!g_serialization)
		{
			_MESSAGE("TCS: SaveCallback aborted — g_serialization is null");
			return;
		}
		if (!g_serialization->OpenRecord(kRecordState, kSaveVersion))
		{
			_MESSAGE("TCS: SaveCallback aborted — OpenRecord failed");
			return;
		}

		g_serialization->WriteRecordData(&g_skillCount, sizeof(g_skillCount));
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			g_serialization->WriteRecordData(&g_skills[i].skillId, sizeof(g_skills[i].skillId));
			g_serialization->WriteRecordData(&g_states[i], sizeof(SkillState));
			_MESSAGE("TCS: SaveCallback wrote skillId=%u major=%u level=%u", g_skills[i].skillId, g_states[i].major, g_states[i].level);
		}
		_MESSAGE("TCS: SaveCallback wrote %u skill state(s)", g_skillCount);

		if (g_serialization->OpenRecord(kRecordRaceBonusApplied, kRaceBonusRecordVersion))
		{
			g_serialization->WriteRecordData(&g_characterCreationBonusesApplied, sizeof(g_characterCreationBonusesApplied));
			_MESSAGE("TCS: SaveCallback wrote raceBonusApplied=%d", g_characterCreationBonusesApplied ? 1 : 0);
		}
		else
		{
			_MESSAGE("TCS: SaveCallback failed to open raceBonusApplied record");
		}

		if (g_serialization->OpenRecord(kRecordSyntheticClasses, kSyntheticClassRecordVersion))
		{
			const UInt32 count = g_classOverrideCount;
			g_serialization->WriteRecordData(&count, sizeof(count));
			for (UInt32 i = 0; i < g_classOverrideCount; ++i)
			{
				SavedSyntheticClassEntry entry{ HashSkillEditorId(g_classOverrides[i].editorId), g_classOverrides[i].tesClass->refID };
				g_serialization->WriteRecordData(&entry, sizeof(entry));
			}
			_MESSAGE("TCS: SaveCallback wrote %u synthetic class entr%s", g_classOverrideCount, g_classOverrideCount == 1 ? "y" : "ies");
		}
		else
		{
			_MESSAGE("TCS: SaveCallback failed to open synthetic class record");
		}

		g_lastKnownPlayerSyntheticClassFormId = 0;
		if (PlayerCharacter* player = GetPlayer())
		{
			if (player->baseForm)
			{
				TESNPC* npc = reinterpret_cast<TESNPC*>(player->baseForm);
				if (npc->npcClass)
				{
					for (UInt32 i = 0; i < g_classOverrideCount; ++i)
					{
						if (g_classOverrides[i].tesClass == npc->npcClass)
						{
							g_lastKnownPlayerSyntheticClassFormId = npc->npcClass->refID;
							break;
						}
					}
				}
			}
		}

		if (g_serialization->OpenRecord(kRecordLastSyntheticClass, kLastSyntheticClassRecordVersion))
		{
			g_serialization->WriteRecordData(&g_lastKnownPlayerSyntheticClassFormId, sizeof(g_lastKnownPlayerSyntheticClassFormId));
			_MESSAGE("TCS: SaveCallback wrote lastKnownPlayerSyntheticClassFormId=%08X", g_lastKnownPlayerSyntheticClassFormId);
		}
		else
		{
			_MESSAGE("TCS: SaveCallback failed to open lastKnownPlayerSyntheticClassFormId record");
		}
	}

	static void LoadCallback(void*)
	{
		std::memset(g_states, 0, sizeof(g_states));
		g_savedSyntheticClasses.clear();
		g_lastKnownPlayerSyntheticClassFormId = 0;

		if (!g_serialization)
		{
			_MESSAGE("TCS: LoadCallback aborted — g_serialization is null");
			return;
		}

		UInt32 type = 0, version = 0, length = 0;
		bool foundRecord = false;
		while (g_serialization->GetNextRecordInfo(&type, &version, &length))
		{
			_MESSAGE("TCS: LoadCallback saw record type=%08X version=%u length=%u (expected type=%08X version=%u)",
				type, version, length, kRecordState, kSaveVersion);

			if (type == kRecordRaceBonusApplied)
			{
				bool savedFlag = false;
				if (g_serialization->ReadRecordData(&savedFlag, sizeof(savedFlag)) == sizeof(savedFlag))
				{
					g_characterCreationBonusesApplied = savedFlag;
					_MESSAGE("TCS: LoadCallback read raceBonusApplied=%d", g_characterCreationBonusesApplied ? 1 : 0);
				}
				continue;
			}

			if (type == kRecordSyntheticClasses)
			{
				if (version != kSyntheticClassRecordVersion)
				{
					_MESSAGE("TCS: LoadCallback ignored incompatible synthetic class record version=%u (expected %u)",
						version, kSyntheticClassRecordVersion);
					continue;
				}

				UInt32 savedCount = 0;
				if (g_serialization->ReadRecordData(&savedCount, sizeof(savedCount)) != sizeof(savedCount))
				{
					_MESSAGE("TCS: LoadCallback failed to read synthetic class count");
					continue;
				}

				const UInt32 maxByLength = (length - sizeof(savedCount)) / sizeof(SavedSyntheticClassEntry);
				const UInt32 entriesToRead = savedCount < maxByLength ? savedCount : maxByLength;
				for (UInt32 i = 0; i < entriesToRead; ++i)
				{
					SavedSyntheticClassEntry entry = {};
					if (g_serialization->ReadRecordData(&entry, sizeof(entry)) != sizeof(entry))
						break;

					g_savedSyntheticClasses.push_back(entry);
					_MESSAGE("TCS: LoadCallback read synthetic class editorIdHash=%08X lastKnownFormId=%08X",
						entry.editorIdHash, entry.lastKnownFormId);
				}

				if (savedCount > entriesToRead)
					_MESSAGE("TCS: synthetic class record count=%u length only contained %u entries", savedCount, entriesToRead);

				continue;
			}

			if (type == kRecordLastSyntheticClass)
			{
				if (version != kLastSyntheticClassRecordVersion)
				{
					_MESSAGE("TCS: LoadCallback ignored incompatible last-synthetic-class record version=%u (expected %u)",
						version, kLastSyntheticClassRecordVersion);
					continue;
				}

				UInt32 savedFormId = 0;
				if (g_serialization->ReadRecordData(&savedFormId, sizeof(savedFormId)) == sizeof(savedFormId))
				{
					g_lastKnownPlayerSyntheticClassFormId = savedFormId;
					_MESSAGE("TCS: LoadCallback read lastKnownPlayerSyntheticClassFormId=%08X", g_lastKnownPlayerSyntheticClassFormId);
				}
				continue;
			}

			if (type != kRecordState || version != kSaveVersion)
				continue;
			foundRecord = true;

			UInt32 savedCount = 0;
			g_serialization->ReadRecordData(&savedCount, sizeof(savedCount));

			UInt32 restored = 0;
			UInt32 orphaned = 0;
			for (UInt32 i = 0; i < savedCount; ++i)
			{
				UInt32 savedSkillId = 0;
				SkillState savedState = {};
				g_serialization->ReadRecordData(&savedSkillId, sizeof(savedSkillId));
				g_serialization->ReadRecordData(&savedState, sizeof(SkillState));
				_MESSAGE("TCS: LoadCallback read skillId=%u major=%u level=%u", savedSkillId, savedState.major, savedState.level);

				const UInt32 index = GetSkillIndexById(savedSkillId);
				if (index < g_skillCount)
				{
					g_states[index] = savedState;
					++restored;

					PushSkillLevelToRealAV(index);
					_MESSAGE("TCS: LoadCallback push readback skillId=%u realActorValue=%08X pushedLevel=%u readback=%u",
						savedSkillId, g_skills[index].realActorValue, g_states[index].level, GetRealAVLevel(index));

					g_postLoadPushCountdown[index] = kPostLoadPushDelay;
				}
				else
				{
					++orphaned;
				}
			}
			_MESSAGE("TCS: save data loaded restored=%u orphaned=%u (skill removed from Skills\\ since save?)", restored, orphaned);
		}
		if (!foundRecord)
			_MESSAGE("TCS: LoadCallback found NO matching record — g_states left at defaults (all major=0)");
	}

	static void NewGameCallback(void*)
	{
		std::memset(g_states, 0, sizeof(g_states));
		std::memset(g_postLoadPushCountdown, 0, sizeof(g_postLoadPushCountdown));
		TCS::LoadClassDefinitionsFromDisk();
	}

	void RegisterSerializationCallbacks()
	{
		if (!g_serialization)
		{
			_MESSAGE("TCS: RegisterSerializationCallbacks aborted — g_serialization is null");
			return;
		}
		_MESSAGE("TCS: registering serialization callbacks, g_pluginHandle=%u g_serialization=%p SaveCallback=%p LoadCallback=%p",
			g_pluginHandle, (void*)g_serialization, (void*)&SaveCallback, (void*)&LoadCallback);
		g_serialization->SetSaveCallback(g_pluginHandle, SaveCallback);
		_MESSAGE("TCS: SetSaveCallback call completed");
		g_serialization->SetPreloadCallback(g_pluginHandle, LoadCallback);
		_MESSAGE("TCS: SetPreloadCallback call completed");
		g_serialization->SetNewGameCallback(g_pluginHandle, NewGameCallback);
		_MESSAGE("TCS: SetNewGameCallback call completed");
	}

	UInt32 TCS_GetSkillActorValue(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		return (index < g_skillCount) ? g_skills[index].realActorValue : 0;
	}

	UInt8 TCS_GetSkillCode(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount)
			return 0;
		UInt8 skillCode = 0;
		ReadXSkillsSkillCode(g_skills[index].realActorValue, skillCode);
		return skillCode;
	}

	bool TCS_IsTCSSkill(const char* editorId)
	{
		return GetSkillIndexByEditorId(editorId) < g_skillCount;
	}

	UInt32 TCS_GetSkillLevel(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		return (index < g_skillCount) ? g_states[index].level : 0;
	}

	bool TCS_IsSkillMajor(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		return (index < g_skillCount) && (g_states[index].major != 0);
	}

	bool TCS_AddSkillXP(const char* editorId, float amount)
	{
		if (!std::isfinite(amount) || amount <= 0.0f)
			return false;

		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount || !g_skills[index].isOwnForm || g_skills[index].realActorValue == 0)
			return false;

		PlayerCharacter* player = GetPlayer();
		if (!player)
			return false;

		typedef void(__thiscall* PlayerModExperienceFn)(PlayerCharacter* thePlayer, UInt32 actorValue, UInt32 useType, float multiplier);
		const UInt32 vtable = *reinterpret_cast<const UInt32*>(player);
		PlayerModExperienceFn fn = *reinterpret_cast<PlayerModExperienceFn*>(vtable + 0x39C);

		constexpr UInt32 kUseType = 0;
		const UInt32 startingLevel = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);

		const UInt32 majorSkillAdvancesBefore = player->majorSkillAdvances;

		fn(player, g_skills[index].realActorValue, kUseType, amount);

		const UInt32 majorSkillAdvancesAfter = player->majorSkillAdvances;
		if (majorSkillAdvancesAfter != majorSkillAdvancesBefore)
		{
			_MESSAGE("TCS: DEBUG TCS_AddSkillXP majorSkillAdvances changed by the engine call itself: before=%u after=%u (BEFORE our own ContributeMajorSkillAdvances/ContributeAttributeBonusBucket run) -- if this fires, those calls are likely now redundant",
				majorSkillAdvancesBefore, majorSkillAdvancesAfter);
		}

		const UInt32 newLevel = static_cast<UInt32>(player->GetAV_F(g_skills[index].realActorValue) + 0.5f);
		float xProgress = 0.0f, xRequired = 0.0f;
		const bool readOk = ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired);

		if (newLevel != g_states[index].level)
			g_states[index].level = newLevel;

		if (newLevel > startingLevel)
		{
			const UInt32 levelUps = newLevel - startingLevel;
			g_states[index].levelUps += levelUps;
			g_states[index].governingAttributeIncreaseCount += levelUps;
			ContributeMajorSkillAdvances(index, levelUps);
			ContributeAttributeBonusBucket(index, levelUps);
		}

		return true;
	}

	bool TCS_SetSkillLevel(const char* editorId, UInt32 level)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount || !g_skills[index].isOwnForm || g_skills[index].realActorValue == 0)
			return false;

		if (level > kMaxSkillLevel)
			level = kMaxSkillLevel;

		const UInt32 previousLevel = g_states[index].level;
		g_states[index].level = level;
		ForceSetSkillLevelOnRealAV(index);

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired) ||
			!std::isfinite(xRequired) || xRequired <= 0.0f)
		{
			xRequired = 1.0f;
		}

		const bool wrote = WriteXSkillsProgress(g_skills[index].realActorValue, 0.0f, xRequired);

		if (level > previousLevel)
		{
			const UInt32 levelUps = level - previousLevel;
			g_states[index].levelUps += levelUps;
			g_states[index].governingAttributeIncreaseCount += levelUps;
			NotifyLevelIncrease(index, previousLevel, levelUps);
			ContributeMajorSkillAdvances(index, levelUps);
			ContributeAttributeBonusBucket(index, levelUps);
		}

		if (wrote)
			_MESSAGE("TCS: TCS_SetSkillLevel editorId=\"%s\" level %u -> %u", editorId, previousLevel, level);
		return wrote;
	}

	float TCS_GetSkillProgress(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount || !g_skills[index].isOwnForm || g_skills[index].realActorValue == 0)
			return 0.0f;

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired))
			return 0.0f;
		return xProgress;
	}

	float TCS_GetSkillRequiredProgress(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount || !g_skills[index].isOwnForm || g_skills[index].realActorValue == 0)
			return 0.0f;

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired))
			return 0.0f;
		return xRequired;
	}

	bool TCS_SetSkillProgress(const char* editorId, float progress)
	{
		if (!std::isfinite(progress) || progress < 0.0f)
			return false;

		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount || !g_skills[index].isOwnForm || g_skills[index].realActorValue == 0)
			return false;

		float xProgress = 0.0f;
		float xRequired = 0.0f;
		if (!ReadXSkillsProgress(g_skills[index].realActorValue, xProgress, xRequired) ||
			!std::isfinite(xRequired) || xRequired <= 0.0f)
		{
			xRequired = 1.0f;
		}

		return WriteXSkillsProgress(g_skills[index].realActorValue, progress, xRequired);
	}

	UInt32 TCS_GetSkillLevelUps(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		return (index < g_skillCount) ? g_states[index].levelUps : 0;
	}

	UInt32 TCS_GetSkillGoverningAttributeIncreases(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		return (index < g_skillCount) ? g_states[index].governingAttributeIncreaseCount : 0;
	}

	UInt32 TCS_GetSkillMastery(const char* editorId)
	{
		const UInt32 index = GetSkillIndexByEditorId(editorId);
		if (index >= g_skillCount)
			return 0;
		return GetSkillMasteryLevel(g_states[index].level);
	}

	const char* TCS_GetSkillDescriptionText(const char* editorId)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
			if (!_stricmp(g_skills[i].editorId.c_str(), editorId))
				return g_skills[i].description.c_str();
		return "";
	}

	const char* TCS_GetSkillLevelQuoteText(const char* editorId, UInt32 idx)
	{
		if (idx >= 4)
			return "";
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (_stricmp(g_skills[i].editorId.c_str(), editorId))
				continue;
			const std::string* tiers[4] = { &g_skills[i].apprenticeText, &g_skills[i].journeymanText, &g_skills[i].expertText, &g_skills[i].masterText };
			return tiers[idx]->c_str();
		}
		return "";
	}

	bool TCS_SetSkillDescriptionText(const char* editorId, const char* text)
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (_stricmp(g_skills[i].editorId.c_str(), editorId))
				continue;
			g_skills[i].description = text;
			return true;
		}
		return false;
	}

	bool TCS_SetSkillLevelQuoteText(const char* editorId, UInt32 idx, const char* text)
	{
		if (idx >= 4)
			return false;
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (_stricmp(g_skills[i].editorId.c_str(), editorId))
				continue;
			std::string* tiers[4] = { &g_skills[i].apprenticeText, &g_skills[i].journeymanText, &g_skills[i].expertText, &g_skills[i].masterText };
			*tiers[idx] = text;
			return true;
		}
		return false;
	}

}