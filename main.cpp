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

	static bool DescriptionFunc_Execute(COMMAND_ARGS, UInt32 mode)
	{
		UInt32 minArgs = (mode == kDescription_Set) ? 1 : 0;

		TESDescription* desc = NULL;
		ExpressionEvaluator eval(PASS_COMMAND_ARGS);
		if (eval.ExtractArgs())
		{
			// TCS dynamic skills: not addressable as a TESForm, so check for a string
			// editorId matching one of our registered skills before falling through
			// to the native TESForm/TESSkill handling below.
			const UInt32 argsAfterMin = eval.NumArgs() - minArgs;
			if (argsAfterMin >= 1 && eval.Arg(minArgs)->CanConvertTo(kTokenType_String))
			{
				const char* maybeSkillEditorId = eval.Arg(minArgs)->GetString();
				if (TCS::TCS_IsTCSSkill(maybeSkillEditorId))
				{
					if (mode == kDescription_Get)
					{
						const char* descText;
						if (argsAfterMin >= 2)
						{
							UInt32 idx = eval.Arg(minArgs + 1)->GetNumber();
							descText = TCS::TCS_GetSkillLevelQuoteText(maybeSkillEditorId, idx);
						}
						else
						{
							descText = TCS::TCS_GetSkillDescriptionText(maybeSkillEditorId);
						}
						AssignToStringVar(PASS_COMMAND_ARGS, descText ? descText : "");
					}
					else if (mode == kDescription_Set && eval.Arg(0)->CanConvertTo(kTokenType_String))
					{
						const char* nuText = eval.Arg(0)->GetString();
						*result = (argsAfterMin >= 2)
							? (TCS::TCS_SetSkillLevelQuoteText(maybeSkillEditorId, eval.Arg(minArgs + 1)->GetNumber(), nuText) ? 1.0 : 0.0)
							: (TCS::TCS_SetSkillDescriptionText(maybeSkillEditorId, nuText) ? 1.0 : 0.0);
					}
					return true;
				}
			}

			switch (argsAfterMin) {
			case 0:
				if (thisObj)
					desc = OBLIVION_CAST(thisObj->baseForm, TESForm, TESDescription);
				break;
			case 1:
				desc = OBLIVION_CAST(eval.Arg(minArgs)->GetTESForm(), TESForm, TESDescription);
				break;
			case 2:
			{
				TESSkill* skill = OBLIVION_CAST(eval.Arg(minArgs)->GetTESForm(), TESForm, TESSkill);
				if (skill) {
					UInt32 idx = eval.Arg(minArgs + 1)->GetNumber();
					if (idx < 4) {
						if (mode == kDescription_Get && !IsDescriptionModified(&skill->levelQuote[idx])) {
							AssignToStringVar(PASS_COMMAND_ARGS, skill->GetLevelQuoteText(idx));
							return true;
						}
						else {
							desc = &skill->levelQuote[idx];
						}
					}
				}
			}
			break;
			}

			if (mode == kDescription_Get) {
				const char* descText = desc ? desc->GetDescription() : "";
				AssignToStringVar(PASS_COMMAND_ARGS, descText);
			}
			else if (mode == kDescription_Set && desc && eval.Arg(0)->CanConvertTo(kTokenType_String)) {
				const char* nuText = eval.Arg(0)->GetString();
				*result = SetDescriptionText(desc, nuText) ? 1.0 : 0.0;
			}
		}

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
		};

		static const Replacement kReplacements[] =
		{
			{ "GetDescription",        Cmd_TCSGetDescription_Execute },
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
					replaced++;
					break;
				}
			}

			cmd++;
		}

		_MESSAGE("Overwrite OBSE Commands: replaced %u of %u",
			replaced, (UInt32)(sizeof(kReplacements) / sizeof(kReplacements[0])));
	}
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
			TCS::LoadClassDefinitionsFromDisk();
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
				if (ReadXSkillsSkillCode(g_skills[i].realActorValue, skillCode)) {
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