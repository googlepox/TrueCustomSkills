#include "Defs.h"
#include "TrueCustomSkillsInterface.h"
#include "obse/CommandTable.h"
#include "obse/ParamInfos.h"

IDebugLog gLog("TrueCustomSkills.log");

PluginHandle g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface* g_serialization = nullptr;
OBSEMessagingInterface* g_messaging = nullptr;

namespace TCS
{

	static TrueCustomSkillsInterface g_tcsInterface =
	{
		TrueCustomSkillsInterface::kInterfaceVersion,
		&TCS_GetSkillActorValue,
		&TCS_GetSkillCode,
		&TCS_IsTCSSkill,
		&TCS_GetSkillLevel,
		&TCS_IsSkillMajor,
		&TCS_AddSkillXP,
		&TCS_SetSkillLevel,
		&TCS_GetSkillProgress,
		&TCS_GetSkillRequiredProgress,
		&TCS_SetSkillProgress,
		&TCS_GetSkillLevelUps,
		&TCS_GetSkillGoverningAttributeIncreases,
		&TCS_GetSkillMastery,
	};

	static bool IsMajorShared(UInt32 skill, TESClass* theClass, TESObjectREFR* thisObj)
	{
		UInt32 customIndex = 0;
		const bool isCustom = TCS::FindSkillIndexBySkillCode(skill, customIndex);
		if (!isCustom && !IsSkill(skill)) return false;

		_MESSAGE("IsMajorShared skill=%08X isCustom=%d idx=%u class=%p obj=%p player=%p",
			skill, isCustom, customIndex, theClass, thisObj, *g_thePlayer);

		if (!theClass)
		{
			if (!thisObj) return false;
			if (isCustom && thisObj == *g_thePlayer)
			{
				return TCS::g_states[customIndex].major != 0;
			}

			TESNPC* npc = OBLIVION_CAST(thisObj->baseForm, TESForm, TESNPC);
			if (!npc || !npc->npcClass) return false;
			theClass = npc->npcClass;
		}

		if (isCustom)
		{
			TESObjectREFR* player = *g_thePlayer;
			if (thisObj == player)
			{
				return TCS::g_states[customIndex].major != 0;
			}
			if (!thisObj && !theClass) return false;

			for (UInt32 i = 0; i < 7; ++i)
				if (theClass->majorSkills[i] == skill) return true;
			
			return false;
		}

		return theClass->IsMajorSkill(skill);
	}

	static bool Cmd_TCSIsMajor_Execute(COMMAND_ARGS)
	{
		*result = 0;

		UInt32 skill = 0;
		TESClass* theClass = NULL;
		ExtractArgs(PASS_EXTRACT_ARGS, &skill, &theClass);

		*result = IsMajorShared(skill, theClass, thisObj) ? 1 : 0;
		return true;
	}

	static bool Cmd_TCSIsMajor_Eval(COMMAND_ARGS_EVAL)
	{
		*result = 0;
		if (!arg1) return true;
		*result = IsMajorShared(*((UInt32*)arg1), (TESClass*)arg2, thisObj) ? 1 : 0;
		return true;
	}

	void OverwriteOBSECommands()
	{
		UInt32 OBSECommandTablePatch = 0x004FCA68;
		CommandInfo* cmd = *(CommandInfo**)(OBSECommandTablePatch + 3);

		if (!cmd)
		{
			_ERROR("Overwrite OBSE Commands: command table not found at %08X",
				OBSECommandTablePatch);
			return;
		}

		_MESSAGE("Overwrite OBSE Commands: Command Table at %08X", cmd);

		struct Replacement
		{
			const char* name;
			bool (*execute)(COMMAND_ARGS);
			bool (*eval)(COMMAND_ARGS_EVAL);
		};

		static const Replacement kReplacements[] =
		{
			{ "IsClassSkillC", Cmd_TCSIsMajor_Execute, Cmd_TCSIsMajor_Eval },
			{ "IsMajorC", Cmd_TCSIsMajor_Execute, Cmd_TCSIsMajor_Eval },
		};

		UInt32 replaced = 0;

		while (cmd->opcode)
		{
			for (const Replacement& r : kReplacements)
			{
				if (_stricmp(cmd->longName, r.name) == 0)
				{
					_MESSAGE("Overwriting command '%s' w/ opcode %08X",
						cmd->longName, cmd->opcode);

					cmd->execute = r.execute;
					cmd->eval = r.eval;
					replaced++;
					break;
				}
			}

			cmd++;
		}

		_MESSAGE("Overwrite OBSE Commands: replaced %u of %u",
			replaced, (UInt32)(sizeof(kReplacements) / sizeof(kReplacements[0])));
	}

	static void UnifiedMessageHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message || !message->data)
			return;

		EditorIDMapper::MessageHandler(message);
	}

	static void MessageHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message)
			return;

		if (message->type == OBSEMessagingInterface::kMessage_PostPostLoad)
		{
			TCS::LoadSkillDefinitionsFromDisk();
			if (!InstallHooks())
				_ERROR("TCS: failed to install native hooks");
		}
		else if (message->type == OBSEMessagingInterface::kMessage_GameInitialized)
		{
			g_messaging->RegisterListener(g_pluginHandle, "EditorIDMapper", UnifiedMessageHandler);
		}
		else if (message->type == OBSEMessagingInterface::kMessage_PostLoadGame)
		{
			OverwriteOBSECommands();
			TCS::LoadClassDefinitionsFromDisk();
		}
	}

	static void InterfaceRequestHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message || !message->data)
			return;

		if (message->type == kMessage_TCSGetInterface)
		{
			*reinterpret_cast<TrueCustomSkillsInterface**>(message->data) = &g_tcsInterface;
		}
	}

	static void RegisterMessaging(const OBSEInterface* obse)
	{
		if (!obse || !obse->QueryInterface || g_pluginHandle == kPluginHandle_Invalid)
			return;

		g_messaging = (OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging);

		if (g_messaging)
		{
			g_messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
			g_messaging->RegisterListener(g_pluginHandle, nullptr, InterfaceRequestHandler);
		}
	}

	bool Cmd_GetTCSSkillCode_Execute(COMMAND_ARGS)
	{
		char editorId[512] = {};
		*result = 0.0;

		if (!ExtractArgs(PASS_EXTRACT_ARGS, &editorId))
			return true;

		for (UInt32 i = 0; i < g_skillCount; ++i)
		{
			if (_stricmp(g_skills[i].editorId.c_str(), editorId) == 0)
			{
				UInt8 skillCode = 0;
				if (ReadXSkillsSkillCode(g_skills[i].realActorValue, skillCode))
				{
					*result = static_cast<double>(skillCode);
					Console_Print("AVCode for %s: %u", editorId, skillCode);
					_MESSAGE("TCS: AVCode for %s: %u", editorId, skillCode);
				}
				break;
			}
		}
		return true;
	}

	DEFINE_COMMAND_PLUGIN(GetTCSSkillCode,
		"returns a True Custom Skills skill's xSkills-internal skill code (for use with MenuQue's own skill commands) by editorId, or 0 if not found/not yet resolvable",
		0, 1, kParams_OneString);
}

extern "C"
{
	bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info)
	{
		if (!obse || !info)
			return false;

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "TrueCustomSkills";
		info->version = TCS::kPluginVersion;
		
		if (obse->isEditor)
			return true;
		if (obse->obseVersion < OBSE_VERSION_INTEGER)
			return false;
		if (obse->oblivionVersion != OBLIVION_VERSION)
			return false;

		g_serialization = static_cast<OBSESerializationInterface*>(obse->QueryInterface(kInterface_Serialization));
		return g_serialization != nullptr;
	}

	bool OBSEPlugin_Load(const OBSEInterface* obse)
	{
		if (!obse)
			return false;

		g_pluginHandle = obse->GetPluginHandle();

		obse->SetOpcodeBase(0x2910);

		if (!obse->isEditor)
		{
			EditorIDMapper::Init((OBSEMessagingInterface*)obse->QueryInterface(kInterface_Messaging), g_pluginHandle);
			TCS::RegisterSerializationCallbacks();
			TCS::RegisterMessaging(obse);
		}

		if (!obse->RegisterCommand(&TCS::kCommandInfo_GetTCSSkillCode))
			_ERROR("TCS: failed to register GetTCSSkillCode command");

		return true;
	}
}