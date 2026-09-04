#include "Defs.h"

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
		return true;
	}
}