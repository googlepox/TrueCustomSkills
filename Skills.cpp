#include "Defs.h"

namespace TCS
{

	SkillDefinition g_skills[kMaxCustomSkills] = {};

	UInt32 g_skillCount = 0;

	SkillState g_states[kMaxCustomSkills] = {};

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

	static void PushSkillLevelToRealAV(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;
		const UInt32 currentAVLevel = static_cast<UInt32>(player->GetActorValue(g_skills[index].realActorValue) + 0.5f);
		const SInt32 delta = static_cast<SInt32>(g_states[index].level) - static_cast<SInt32>(currentAVLevel);
		if (delta != 0)
			player->ModBaseAV(g_skills[index].realActorValue, delta);
	}

	void ReconcileSkillLevelWithRealAV(UInt32 index)
	{
		if (index >= g_skillCount || g_skills[index].realActorValue == 0)
		{
			_MESSAGE("TCS: ReconcileSkillLevelWithRealAV skillId=%u SKIPPED — index/realActorValue check failed (realActorValue=%u)",
				index < g_skillCount ? g_skills[index].skillId : 0, index < g_skillCount ? g_skills[index].realActorValue : 0);
			return;
		}
		PlayerCharacter* player = GetPlayer();
		if (!player)
		{
			_MESSAGE("TCS: ReconcileSkillLevelWithRealAV skillId=%u SKIPPED — GetPlayer() returned null", g_skills[index].skillId);
			return;
		}

		NormalizeState(index);
		SkillState& state = g_states[index];
		const UInt32 avLevel = static_cast<UInt32>(player->GetActorValue(g_skills[index].realActorValue) + 0.5f);

		_MESSAGE("TCS: ReconcileSkillLevelWithRealAV skillId=%u realActorValue=%08X avLevel=%u stateLevel=%u",
			g_skills[index].skillId, g_skills[index].realActorValue, avLevel, state.level);

		if (avLevel > state.level)
		{
			state.level = (avLevel > kMaxSkillLevel) ? kMaxSkillLevel : avLevel;
			state.progress = 0.0f;
			_MESSAGE("TCS: ReconcileSkillLevelWithRealAV skillId=%u AV was higher, adopted avLevel=%u", g_skills[index].skillId, state.level);
		}
	}

	void EnsureCustomActorValuesRegistered()
	{
		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (g_skills[i].realActorValue != 0)
				continue;

			bool wasReused = false;
			g_skills[i].realActorValue = RegisterCustomActorValue(g_skills[i].name, wasReused);

			if (g_skills[i].realActorValue == 0)
				continue;

			if (wasReused)
			{
				PlayerCharacter* player = GetPlayer();
				if (player)
				{
					NormalizeState(i);
					UInt32 avLevel = static_cast<UInt32>(player->GetActorValue(g_skills[i].realActorValue) + 0.5f);
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
		}
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
			EnsureDummySkillRegistered();
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
		if (g_skillCount == 0)
			EnsureDummySkillRegistered();
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

	const char* GetSkillIconLarge(UInt32 index)
	{
		if (index >= g_skillCount)
			return "";
		const std::string& path = g_skills[index].iconLarge;
		return LooseTextureAssetExists(path) ? path.c_str() : "";
	}

	const char* GetSkillIconSmall(UInt32 index)
	{
		if (index >= g_skillCount)
			return "";
		const std::string& smallPath = g_skills[index].iconSmall;
		if (LooseTextureAssetExists(smallPath))
			return smallPath.c_str();
		return GetSkillIconLarge(index);
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

	static void MirrorLevelUpSideEffects(UInt32 index, UInt32 levelUps)
	{
		if (!levelUps)
			return;
		PlayerCharacter* player = GetPlayer();
		if (!player)
			return;

		for (UInt32 i = 0; i < levelUps; ++i)
		{
			if (g_skills[index].governingAttributeAV <= kActorVal_Luck)
				reinterpret_cast<PlayerIncrementAttributeBonusBucketFn>(kPlayerIncrementAttributeBonusBucket)(player, g_skills[index].governingAttributeAV);

			if (IsEffectiveMajor(index))
			{
				++player->majorSkillAdvances;
				const UInt32 levelUpSkillCount = GetLevelUpSkillCount();
				if (levelUpSkillCount)
					reinterpret_cast<PlayerMaybeStartNextAttributeBonusBucketFn>(kPlayerMaybeStartNextAttributeBonusBucket)(player);
				if (levelUpSkillCount && player->majorSkillAdvances >= levelUpSkillCount)
					player->bCanLevelUp = 1;
			}
		}
	}

	static bool ShowSkillPerkPopup(UInt32 index, UInt32 mastery)
	{
		if (index >= g_skillCount || mastery == 0)
			return false;

		const char* icon = GetSkillIconLarge(index);
		const char* description = g_skills[index].description.c_str();
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

	static void NotifyLevelIncrease(UInt32 index, UInt32 previousLevel, UInt32 levelUps)
	{
		if (!levelUps || index >= g_skillCount)
			return;

		char message[256] = {};
		_snprintf_s(message, sizeof(message), _TRUNCATE, "Your %s skill increased to %u.",
			g_skills[index].name.c_str(), g_states[index].level);
		::QueueUIMessage(message, 0, 1, 2.0f);

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

		const UInt32 previousLevel = state.level;
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
			NotifyLevelIncrease(index, previousLevel, levelUps);
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
	}

}