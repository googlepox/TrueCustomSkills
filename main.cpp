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