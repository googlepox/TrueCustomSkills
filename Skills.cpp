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

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
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

	bool EnsureCustomActorValuesRegistered()
	{
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
	}

	static void LoadCallback(void*)
	{
		std::memset(g_states, 0, sizeof(g_states));
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

}