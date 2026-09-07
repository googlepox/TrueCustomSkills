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

	bool(*SetSkillLevel)(const char* editorId, UInt32 level);

	float(*GetSkillProgress)(const char* editorId);

	float(*GetSkillRequiredProgress)(const char* editorId);

	bool(*SetSkillProgress)(const char* editorId, float progress);

	UInt32(*GetSkillLevelUps)(const char* editorId);

	UInt32(*GetSkillGoverningAttributeIncreases)(const char* editorId);

	UInt32(*GetSkillMastery)(const char* editorId);
};