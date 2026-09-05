#include "Defs.h"
#include "obse/CommandTable.h"
#include "obse/ParamInfos.h"

IDebugLog gLog("TrueCustomSkills.log");

PluginHandle g_pluginHandle = kPluginHandle_Invalid;
OBSESerializationInterface* g_serialization = nullptr;

namespace TCS
{

	static void MessageHandler(OBSEMessagingInterface::Message* message)
	{
		if (!message)
			return;

		if (message->type == OBSEMessagingInterface::kMessage_PostPostLoad)
		{
			if (!InstallHooks())
				_ERROR("TCS: failed to install native hooks");
		}
	}

	static void RegisterMessaging(const OBSEInterface* obse)
	{
		if (!obse || !obse->QueryInterface || g_pluginHandle == kPluginHandle_Invalid)
			return;

		OBSEMessagingInterface* messaging =
			static_cast<OBSEMessagingInterface*>(obse->QueryInterface(kInterface_Messaging));
		if (messaging && messaging->RegisterListener)
			messaging->RegisterListener(g_pluginHandle, "OBSE", MessageHandler);
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
		info->name = "True Custom Skills";
		info->version = TCS::kPluginVersion;

		if (obse->isEditor)
			return false;
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

		TCS::RegisterSerializationCallbacks();
		TCS::LoadSkillDefinitionsFromDisk();
		TCS::RegisterMessaging(obse);

		if (!obse->RegisterCommand(&TCS::kCommandInfo_GetTCSSkillCode))
			_ERROR("TCS: failed to register GetTCSSkillCode command");

		return true;
	}
}