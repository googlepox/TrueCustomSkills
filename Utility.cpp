#include "Defs.h"

namespace TCS
{

	bool LooseTextureAssetExists(const std::string& relativePath)
	{
		if (relativePath.empty())
			return false;
		const std::string fullPath = "Data\\" + relativePath;
		const DWORD attrs = GetFileAttributesA(fullPath.c_str());
		return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
	}

	UInt32 g_appliedPatches = 0;

	UInt32 g_failedPatches = 0;

	static bool BytesEqual(UInt32 address, const UInt8* expected, UInt32 length)
	{
		return std::memcmp(reinterpret_cast<const void*>(address), expected, length) == 0;
	}

	static UInt32 ReadRelCallTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static UInt32 ReadRelJumpTarget(UInt32 address)
	{
		const SInt32 offset = *reinterpret_cast<const SInt32*>(address + 1);
		return address + 5 + offset;
	}

	static bool WriteRelCallChecked(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("TCS: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			_ERROR("TCS: %s target mismatch at %08X expected %08X actual %08X", name, address, expectedTarget, currentTarget);
			++g_failedPatches;
			return false;
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	bool WriteRelCallChained(const char* name, UInt32 address, UInt32 expectedTarget, UInt32 hookTarget, UInt32& originalTarget)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] != 0xE8)
		{
			_ERROR("TCS: %s at %08X is not a call", name, address);
			++g_failedPatches;
			return false;
		}

		const UInt32 currentTarget = ReadRelCallTarget(address);
		if (currentTarget == hookTarget)
			return true;
		if (currentTarget != expectedTarget)
		{
			if (originalTarget != expectedTarget && originalTarget != currentTarget)
			{
				_ERROR("TCS: %s target mismatch at %08X expected %08X or chained %08X actual %08X",
					name, address, expectedTarget, originalTarget, currentTarget);
				++g_failedPatches;
				return false;
			}

			originalTarget = currentTarget;
			_MESSAGE("TCS: chaining existing %s target=%08X", name, currentTarget);
		}

		WriteRelCall(address, hookTarget);
		++g_appliedPatches;
		return true;
	}

	static void WriteRelJumpBytes(UInt8* code, UInt32 address, UInt32 target, UInt32 patchLength)
	{
		code[0] = 0xE9;
		*reinterpret_cast<UInt32*>(code + 1) = target - address - 5;
		for (UInt32 i = 5; i < patchLength; ++i)
			code[i] = 0x90;
	}

	bool WriteRelJumpChecked(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9 && ReadRelJumpTarget(address) == target)
			return true;
		if (!BytesEqual(address, expected, expectedLength))
		{
			_ERROR("TCS: signature mismatch for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("TCS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			++g_failedPatches;
			return false;
		}

		WriteRelJumpBytes(reinterpret_cast<UInt8*>(ptr), address, target, patchLength);
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		_MESSAGE("TCS: installed %s at %08X", name, address);
		return true;
	}

	static bool WriteRelJumpRaw(const char* name, UInt32 address, UInt32 target, UInt32 patchLength = 5)
	{
		if (patchLength < 5)
		{
			_ERROR("TCS: invalid raw jump patch length for %s at %08X length=%u", name, address, patchLength);
			++g_failedPatches;
			return false;
		}

		DWORD oldProtect = 0;
		void* ptr = reinterpret_cast<void*>(address);
		if (!VirtualProtect(ptr, patchLength, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			_ERROR("TCS: VirtualProtect failed for %s at %08X gle=%u", name, address, GetLastError());
			++g_failedPatches;
			return false;
		}

		WriteRelJumpBytes(reinterpret_cast<UInt8*>(ptr), address, target, patchLength);
		FlushInstructionCache(GetCurrentProcess(), ptr, patchLength);
		DWORD ignored = 0;
		VirtualProtect(ptr, patchLength, oldProtect, &ignored);
		++g_appliedPatches;
		_MESSAGE("TCS: installed %s (chained) at %08X", name, address);
		return true;
	}

	static void* CreateTrampoline(UInt32 address, UInt32 stolenLength)
	{
		const UInt32 length = stolenLength + 5;
		UInt8* trampoline = static_cast<UInt8*>(VirtualAlloc(nullptr, length, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
		if (!trampoline)
			return nullptr;

		std::memcpy(trampoline, reinterpret_cast<const void*>(address), stolenLength);
		trampoline[stolenLength] = 0xE9;
		*reinterpret_cast<UInt32*>(trampoline + stolenLength + 1) =
			(address + stolenLength) - (reinterpret_cast<UInt32>(trampoline) + stolenLength) - 5;
		FlushInstructionCache(GetCurrentProcess(), trampoline, length);
		return trampoline;
	}

	bool InstallFunctionJumpHook(const char* name, UInt32 address, const UInt8* expected, UInt32 expectedLength, UInt32 target, UInt32 patchLength, void*& original)
	{
		const UInt8* actual = reinterpret_cast<const UInt8*>(address);
		if (actual[0] == 0xE9)
		{
			const UInt32 currentTarget = ReadRelJumpTarget(address);
			if (currentTarget == target)
				return true;

			original = reinterpret_cast<void*>(currentTarget);
			_MESSAGE("TCS: chaining existing %s target=%08X", name, currentTarget);
			return WriteRelJumpRaw(name, address, target);
		}

		if (!original)
			original = CreateTrampoline(address, patchLength);
		if (!original)
		{
			_ERROR("TCS: failed to create trampoline for %s at %08X", name, address);
			++g_failedPatches;
			return false;
		}

		return WriteRelJumpChecked(name, address, expected, expectedLength, target, patchLength);
	}

	[[nodiscard]] __declspec(noinline) UInt32 __stdcall DetourVtable(UInt32 addr, UInt32 dst)
	{
		UInt32 originalFunction = *(UInt32*)addr;
		SafeWrite32(addr, dst);
		return originalFunction;
	}

	void SetTileString(Tile* tile, UInt32 trait, const char* value)
	{
		if (tile)
			reinterpret_cast<void(__thiscall*)(Tile*, UInt32, const char*)>(kTileSetString)(tile, trait, value ? value : "");
	}

	float GetTileFloat(Tile* tile, UInt32 trait)
	{
		return tile ? static_cast<float>(reinterpret_cast<double(__thiscall*)(Tile*, UInt32)>(kTileGetFloat)(tile, trait)) : 0.0f;
	}

	void SetTileFloat(Tile* tile, UInt32 trait, float value)
	{
		if (tile)
			reinterpret_cast<void(__thiscall*)(Tile*, UInt32, float)>(kTileSetFloat)(tile, trait, value);
	}

	Tile* FindChildTileById(Tile* parent, UInt32 id)
	{
		if (!parent)
			return nullptr;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(parent) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile && static_cast<UInt32>(GetTileFloat(tile, kTileValue_id) + 0.5f) == id)
				return tile;

			node = *reinterpret_cast<UInt32*>(node);
		}
		return nullptr;
	}

	Tile* FindDescendantTileById(Tile* root, UInt32 id, UInt32 maxDepth)
	{
		if (!root || maxDepth == 0)
			return nullptr;

		UInt32 node = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(root) + 0x34);
		while (node)
		{
			Tile* tile = *reinterpret_cast<Tile**>(node + 8);
			if (tile)
			{
				if (static_cast<UInt32>(GetTileFloat(tile, kTileValue_id) + 0.5f) == id)
					return tile;
				if (Tile* found = FindDescendantTileById(tile, id, maxDepth - 1))
					return found;
			}
			node = *reinterpret_cast<UInt32*>(node);
		}
		return nullptr;
	}

	MenuQueInsertXMLFn g_menuQueInsertXML = nullptr;

	static bool g_menuQueResolveAttempted = false;

	bool ResolveMenuQueInsertXML()
	{
		if (g_menuQueResolveAttempted)
			return g_menuQueInsertXML != nullptr;
		g_menuQueResolveAttempted = true;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE)
			return false;

		MODULEENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		HMODULE menuQueModule = nullptr;

		if (Module32First(snapshot, &entry))
		{
			do
			{
				char lowerName[MAX_MODULE_NAME32 + 1] = {};
				for (UInt32 i = 0; entry.szModule[i] && i < MAX_MODULE_NAME32; ++i)
					lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(entry.szModule[i])));

				if (std::strstr(lowerName, "submodule"))
				{
					menuQueModule = entry.hModule;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (!menuQueModule)
		{
			_MESSAGE("TCS: Submodule.Game (MenuQue) not found — DarN scrollbar patch will use SetTileFloat fallback only");
			return false;
		}

		g_menuQueInsertXML = reinterpret_cast<MenuQueInsertXMLFn>(reinterpret_cast<UInt8*>(menuQueModule) + kMenuQueInsertXMLRVA);
		_MESSAGE("TCS: resolved MenuQue InsertXML at %p (module base %p)", (void*)g_menuQueInsertXML, (void*)menuQueModule);
		return true;
	}

	void* g_avTokenRegister = nullptr;
	void* g_avTokenLookupNext = nullptr;
	static bool g_aavResolveAttempted = false;

	bool ResolveAddActorValues()
	{
		if (g_aavResolveAttempted)
			return g_avTokenRegister != nullptr;
		g_aavResolveAttempted = true;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE)
			return false;

		MODULEENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		HMODULE aavModule = nullptr;

		if (Module32First(snapshot, &entry))
		{
			do
			{
				char lowerName[MAX_MODULE_NAME32 + 1] = {};
				for (UInt32 i = 0; entry.szModule[i] && i < MAX_MODULE_NAME32; ++i)
					lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(entry.szModule[i])));

				if (std::strstr(lowerName, "addactorvalues"))
				{
					aavModule = entry.hModule;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (!aavModule)
		{
			_MESSAGE("TCS: AddActorValues not found — custom skills will use placeholder AVs only, no genuine cross-mod AV compatibility");
			return false;
		}

		g_avTokenRegister = reinterpret_cast<UInt8*>(aavModule) + kAVTokenRegisterRVA;
		g_avTokenLookupNext = reinterpret_cast<UInt8*>(aavModule) + kAVTokenLookupNextRVA;
		_MESSAGE("TCS: resolved AVToken::Register at %p, LookupNext at %p (module base %p)",
			g_avTokenRegister, g_avTokenLookupNext, (void*)aavModule);
		return true;
	}

	void* g_xSkillsLinkFormEx = nullptr;
	static bool g_xSkillsResolveAttempted = false;

	bool ResolveXSkillsLinkFormEx()
	{
		if (g_xSkillsResolveAttempted)
			return g_xSkillsLinkFormEx != nullptr;
		g_xSkillsResolveAttempted = true;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE)
			return false;

		MODULEENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		HMODULE xSkillsModule = nullptr;

		if (Module32First(snapshot, &entry))
		{
			do
			{
				char lowerName[MAX_MODULE_NAME32 + 1] = {};
				for (UInt32 i = 0; entry.szModule[i] && i < MAX_MODULE_NAME32; ++i)
					lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(entry.szModule[i])));

				if (std::strstr(lowerName, "submodule"))
				{
					xSkillsModule = entry.hModule;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (!xSkillsModule)
		{
			_MESSAGE("TCS: Submodule.Game (xSkills) not found -- LinkFormEx test unavailable");
			return false;
		}

		g_xSkillsLinkFormEx = reinterpret_cast<UInt8*>(xSkillsModule) + kXSkillsLinkFormExRVA;
		_MESSAGE("TCS: resolved xSkills LinkFormEx at %p (module base %p)", g_xSkillsLinkFormEx, (void*)xSkillsModule);
		return true;
	}

	bool LinkSkillWithXSkills(UInt32 avCode, const char* skillName)
	{
		if (!ResolveXSkillsLinkFormEx())
		{
			_MESSAGE("TCS: LinkSkillWithXSkills aborted for \"%s\" -- xSkills not resolved", skillName);
			return false;
		}

		TESForm* form = ::CreateFormInstance(kFormType_Skill);
		if (!form)
		{
			_MESSAGE("TCS: LinkSkillWithXSkills aborted for \"%s\" -- CreateFormInstance(kFormType_Skill) failed", skillName);
			return false;
		}

		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(form) + kTESSkillExActorValueOffset) = avCode;

		const char result = reinterpret_cast<LinkFormExFn>(g_xSkillsLinkFormEx)(form);

		_MESSAGE("TCS: LinkSkillWithXSkills LinkFormEx returned %d for \"%s\" formID=%08X avCode=%08X (check xSkills' OWN log for the real verdict)",
			(int)result, skillName, form->refID, avCode);
		return result != 0;
	}

	void* g_xSkillsTreeFind = nullptr;
	void* g_xSkillsSkillMap = nullptr;
	static bool g_xSkillsSkillMapResolveAttempted = false;

	bool ResolveXSkillsSkillMap()
	{
		if (g_xSkillsSkillMapResolveAttempted)
			return g_xSkillsTreeFind != nullptr && g_xSkillsSkillMap != nullptr;
		g_xSkillsSkillMapResolveAttempted = true;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE)
			return false;

		MODULEENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		HMODULE xSkillsModule = nullptr;

		if (Module32First(snapshot, &entry))
		{
			do
			{
				char lowerName[MAX_MODULE_NAME32 + 1] = {};
				for (UInt32 i = 0; entry.szModule[i] && i < MAX_MODULE_NAME32; ++i)
					lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(entry.szModule[i])));

				if (std::strstr(lowerName, "submodule"))
				{
					xSkillsModule = entry.hModule;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (!xSkillsModule)
		{
			_MESSAGE("TCS: Submodule.Game (xSkills) not found -- external progress reading unavailable");
			return false;
		}

		g_xSkillsTreeFind = reinterpret_cast<UInt8*>(xSkillsModule) + kXSkillsTreeFindRVA;
		g_xSkillsSkillMap = reinterpret_cast<UInt8*>(xSkillsModule) + kXSkillsSkillMapRVA;
		_MESSAGE("TCS: resolved xSkills skill map at %p, tree-find at %p (module base %p)",
			g_xSkillsSkillMap, g_xSkillsTreeFind, (void*)xSkillsModule);
		return true;
	}

	void* g_xSkillsFormMap = nullptr;
	static bool g_xSkillsFormMapResolveAttempted = false;

	bool ResolveXSkillsFormMap()
	{
		if (g_xSkillsFormMapResolveAttempted)
			return g_xSkillsTreeFind != nullptr && g_xSkillsFormMap != nullptr;
		g_xSkillsFormMapResolveAttempted = true;

		HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
		if (snapshot == INVALID_HANDLE_VALUE)
			return false;

		MODULEENTRY32 entry = {};
		entry.dwSize = sizeof(entry);
		HMODULE xSkillsModule = nullptr;

		if (Module32First(snapshot, &entry))
		{
			do
			{
				char lowerName[MAX_MODULE_NAME32 + 1] = {};
				for (UInt32 i = 0; entry.szModule[i] && i < MAX_MODULE_NAME32; ++i)
					lowerName[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(entry.szModule[i])));

				if (std::strstr(lowerName, "submodule"))
				{
					xSkillsModule = entry.hModule;
					break;
				}
			} while (Module32Next(snapshot, &entry));
		}
		CloseHandle(snapshot);

		if (!xSkillsModule)
		{
			_MESSAGE("TCS: Submodule.Game (xSkills) not found -- form map unavailable");
			return false;
		}

		g_xSkillsTreeFind = reinterpret_cast<UInt8*>(xSkillsModule) + kXSkillsTreeFindRVA;
		g_xSkillsFormMap = reinterpret_cast<UInt8*>(xSkillsModule) + kXSkillsFormMapRVA;
		_MESSAGE("TCS: resolved xSkills form map at %p, tree-find at %p (module base %p)",
			g_xSkillsFormMap, g_xSkillsTreeFind, (void*)xSkillsModule);
		return true;
	}

	static void* FindXSkillsRecordForAV(UInt32 avCode)
	{
		if (!ResolveXSkillsSkillMap())
			return nullptr;

		void* outSlot = nullptr;
		UInt32 key = avCode;
		reinterpret_cast<XSkillsTreeFindFn>(g_xSkillsTreeFind)(g_xSkillsSkillMap, &outSlot, &key);

		void* sentinel = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(g_xSkillsSkillMap) + 4);
		if (!outSlot || outSlot == sentinel)
			return nullptr;

		if (IsBadReadPtr(outSlot, 20))
			return nullptr;

		void* record = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(outSlot) + 16);
		if (!record || IsBadReadPtr(record, 9)) 
			return nullptr;

		return record;
	}

	static void* FindXSkillsFormForAV(UInt32 avCode)
	{
		if (!ResolveXSkillsFormMap())
			return nullptr;

		void* outSlot = nullptr;
		UInt32 key = avCode;
		reinterpret_cast<XSkillsTreeFindFn>(g_xSkillsTreeFind)(g_xSkillsFormMap, &outSlot, &key);

		void* sentinel = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(g_xSkillsFormMap) + 4);
		if (!outSlot || outSlot == sentinel)
			return nullptr;

		if (IsBadReadPtr(outSlot, 20))
			return nullptr;

		void* form = *reinterpret_cast<void**>(reinterpret_cast<UInt8*>(outSlot) + 16);
		if (!form || IsBadReadPtr(form, 0x38))
			return nullptr;

		return form;
	}

	void* GetXSkillsFormForAV(UInt32 avCode)
	{
		return FindXSkillsFormForAV(avCode);
	}

	bool SetXSkillsGoverningAttributeAndSpecialization(UInt32 avCode, UInt32 governingAttribute, UInt32 specialization)
	{
		if (governingAttribute >= 8 || specialization >= 3)
			return false;

		void* form = FindXSkillsFormForAV(avCode);
		if (!form || IsBadWritePtr(reinterpret_cast<UInt8*>(form) + 0x30, 4) || IsBadWritePtr(reinterpret_cast<UInt8*>(form) + 0x34, 4))
			return false;

		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(form) + 0x30) = governingAttribute;
		*reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(form) + 0x34) = specialization;
		return true;
	}

	bool SetXSkillsIcon(UInt32 avCode, const std::string& iconPath)
	{
		void* form = FindXSkillsFormForAV(avCode);
		if (!form || IsBadWritePtr(reinterpret_cast<UInt8*>(form) + 0x20, 8))
			return false;

		reinterpret_cast<TESIcon*>(reinterpret_cast<UInt8*>(form) + 0x20)->SetPath(iconPath.c_str());
		return true;
	}

	bool ReadXSkillsSkillCode(UInt32 avCode, UInt8& outSkillCode)
	{
		void* record = FindXSkillsRecordForAV(avCode);
		if (!record)
			return false;
		outSkillCode = *(reinterpret_cast<UInt8*>(record) + 8);
		return true;
	}

	static constexpr bool kEnableXSkillsProgressReconcile = false;

	bool ReadXSkillsProgress(UInt32 avCode, float& outProgress, float& outRequired)
	{
		if (!kEnableXSkillsProgressReconcile)
			return false;

		void* record = FindXSkillsRecordForAV(avCode);
		if (!record)
			return false;

		if (IsBadReadPtr(record, kXSkillsRecordRequiredProgressOffset + 4))
		{
			_MESSAGE("TCS: ReadXSkillsProgress ABORT — record=%p fails extended-range IsBadReadPtr", record);
			return false;
		}
		_MESSAGE("TCS: ReadXSkillsProgress record=%p for avCode=%08X", record, avCode);

		auto resolveField = [](void* record, UInt32 fieldOffset, float& outValue) -> bool
			{
				const UInt32 rawField = *reinterpret_cast<UInt32*>(reinterpret_cast<UInt8*>(record) + fieldOffset);
				void* asPointer = reinterpret_cast<void*>(rawField);
				if (asPointer && !IsBadReadPtr(asPointer, 4))
				{
					outValue = *reinterpret_cast<float*>(asPointer);
					return true;
				}

				outValue = *reinterpret_cast<float*>(reinterpret_cast<UInt8*>(record) + fieldOffset);
				return std::isfinite(outValue);
			};

		float progressValue = 0.0f, requiredValue = 0.0f;
		const bool gotProgress = resolveField(record, kXSkillsRecordProgressOffset, progressValue);
		const bool gotRequired = resolveField(record, kXSkillsRecordRequiredProgressOffset, requiredValue);
		_MESSAGE("TCS: ReadXSkillsProgress step4 progress=%.2f (ok=%d) required=%.2f (ok=%d)",
			progressValue, gotProgress ? 1 : 0, requiredValue, gotRequired ? 1 : 0);

		if (!gotProgress || !gotRequired)
		{
			_MESSAGE("TCS: ReadXSkillsProgress ABORT — could not resolve one or both fields to a sane value");
			return false;
		}

		outProgress = progressValue;
		outRequired = requiredValue;
		_MESSAGE("TCS: ReadXSkillsProgress step5 SUCCESS progress=%.2f required=%.2f", outProgress, outRequired);
		return true;
	}

}