#pragma once

#include "obse/PluginAPI.h"

static const UInt32 kMessage_TCSGetInterface = 'TCS_';

struct TrueCustomSkillsInterface
{
	enum { kInterfaceVersion = 1 };

	UInt32 interfaceVersion;

	UInt32(*GetSkillActorValue)(const char* editorId);

	UInt8(*GetSkillCode)(const char* editorId);

	bool(*IsTCSSkill)(const char* editorId);

	UInt32(*GetSkillLevel)(const char* editorId);

	bool(*IsSkillMajor)(const char* editorId);

	bool(*AddSkillXP)(const char* editorId, float amount);
};