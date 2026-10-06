#include "LuaBridge.h"

#include "core/File.h"
#include "core/SettingsIni.h"
#include "DebugLog.h"
#include "item/Item.h"
#include "skill/Skill.h"

struct JobNameEntry {
	int job;
	const char* name;
};

#include "../session/JobNameTable.generated.inc"
#include "../skill/SkillEnumIdTable.inc"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <cmath>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {

constexpr bool kLogLuaSyntheticGlobals = false;

#define LOG_LUA_SYNTHETIC_GLOBAL(...) do { if constexpr (kLogLuaSyntheticGlobals) { DbgLog(__VA_ARGS__); } } while (0)

std::string NormalizeSlashPath(std::string path)
{
	std::replace(path.begin(), path.end(), '/', '\\');
	return path;
}

int GetAbsoluteLuaIndex(lua_State* state, int index)
{
	if (!state || index > 0 || index <= LUA_REGISTRYINDEX) {
		return index;
	}

	return lua_gettop(state) + index + 1;
}

bool PathEndsWithInsensitive(const std::string& path, const char* suffix)
{
	if (!suffix) {
		return false;
	}

	const size_t suffixLength = std::strlen(suffix);
	if (path.size() < suffixLength) {
		return false;
	}

	return _stricmp(path.c_str() + (path.size() - suffixLength), suffix) == 0;
}

const char* GetReturnedTableBindingName(const std::string& path)
{
	if (PathEndsWithInsensitive(path, "iteminfo.lub")
		|| PathEndsWithInsensitive(path, "iteminfo.lua")
		|| PathEndsWithInsensitive(path, "iteminfo_true.lub")
		|| PathEndsWithInsensitive(path, "iteminfo_true.lua")) {
		return "OpenMidgardItemInfo";
	}

	if (PathEndsWithInsensitive(path, "lua files\\datainfo\\jobidentity.lub")
		|| PathEndsWithInsensitive(path, "lua files\\datainfo\\jobidentity_f.lub")) {
		return "JobIdentity";
	}

	return nullptr;
}

bool IsAccessoryNameTableScript(const std::string& path)
{
	return PathEndsWithInsensitive(path, "lua files\\datainfo\\accname.lub")
		|| PathEndsWithInsensitive(path, "lua files\\datainfo\\accname_f.lub")
		|| PathEndsWithInsensitive(path, "lua files\\datainfo\\accname_eng.lub");
}

void SetLuaGlobalInteger(lua_State* state, const char* name, int value)
{
	if (!state || !name || !*name) {
		return;
	}

	lua_pushinteger(state, value);
	lua_setglobal(state, name);
}

bool TryGetLuaGlobalInteger(lua_State* state, const char* name, int* outValue)
{
	if (!state || !name || !*name || !outValue) {
		return false;
	}

	lua_getglobal(state, name);
	if (!lua_isnumber(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(state, -1));
	lua_pop(state, 1);
	return true;
}

bool TryGetGeneratedJobIdFromGlobalName(const char* globalName, int* outValue)
{
	if (!globalName || std::strncmp(globalName, "JT_", 3) != 0 || !outValue) {
		return false;
	}

	const char* jobName = globalName + 3;
	if (!*jobName) {
		return false;
	}

	for (const JobNameEntry& entry : kGeneratedJobNames) {
		if (entry.name && std::strcmp(entry.name, jobName) == 0) {
			*outValue = entry.job;
			return true;
		}
	}

	return false;
}

bool TryGetLuaTableIntegerField(lua_State* state, int tableIndex, const char* fieldName, int* outValue)
{
	if (!state || !fieldName || !*fieldName || !outValue) {
		return false;
	}

	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (!lua_isnumber(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(state, -1));
	lua_pop(state, 1);
	return true;
}

bool TryGetLuaJobEnumValue(lua_State* state, const char* name, int* outValue)
{
	lua_getglobal(state, "jobtbl");
	const bool found = lua_istable(state, -1)
		&& TryGetLuaTableIntegerField(state, -1, name, outValue);
	lua_pop(state, 1);
	return found || TryGetLuaGlobalInteger(state, name, outValue);
}

bool TryGetLuaTableBooleanField(lua_State* state, int tableIndex, const char* fieldName, bool* outValue)
{
	if (!state || !fieldName || !*fieldName || !outValue) {
		return false;
	}

	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (lua_isboolean(state, -1)) {
		*outValue = lua_toboolean(state, -1) != 0;
		lua_pop(state, 1);
		return true;
	}
	if (lua_isnumber(state, -1)) {
		*outValue = lua_tointeger(state, -1) != 0;
		lua_pop(state, 1);
		return true;
	}

	lua_pop(state, 1);
	return false;
}

bool TryGetLuaTableIntegerArrayField(lua_State* state, int tableIndex, const char* fieldName, std::vector<int>* outValues)
{
	if (!state || !fieldName || !*fieldName || !outValues) {
		return false;
	}

	outValues->clear();
	const int absoluteIndex = GetAbsoluteLuaIndex(state, tableIndex);
	lua_getfield(state, absoluteIndex, fieldName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	const int fieldIndex = GetAbsoluteLuaIndex(state, -1);
	const size_t count = static_cast<size_t>(lua_objlen(state, fieldIndex));
	for (size_t index = 1; index <= count; ++index) {
		lua_rawgeti(state, fieldIndex, static_cast<int>(index));
		if (lua_isnumber(state, -1)) {
			outValues->push_back(static_cast<int>(lua_tointeger(state, -1)));
		}
		lua_pop(state, 1);
	}

	lua_pop(state, 1);
	return !outValues->empty();
}

int AllocateSyntheticLuaTableValue(const char* tableName, const char* keyName)
{
	if (!tableName || !*tableName || !keyName || !*keyName) {
		return 0;
	}

	static std::unordered_map<std::string, int> syntheticValues;
	static int nextSkillId = 50000;
	static int nextEffectId = 70000;
	static int nextActorState = 90000;
	static int nextJobEnum = 110000;

	std::string compositeKey = tableName;
	compositeKey += '.';
	compositeKey += keyName;

	const auto existing = syntheticValues.find(compositeKey);
	if (existing != syntheticValues.end()) {
		return existing->second;
	}

	int value = 0;
	if (_stricmp(tableName, "SKID") == 0) {
		value = nextSkillId++;
	} else if (_stricmp(tableName, "EFID") == 0) {
		value = nextEffectId++;
	} else if (_stricmp(tableName, "ACTOR_STATE") == 0) {
		value = nextActorState++;
	} else if (_stricmp(tableName, "JT_ENUM") == 0) {
		value = nextJobEnum++;
	}

	if (value != 0) {
		syntheticValues.emplace(compositeKey, value);
	}

	return value;
}

bool HasLuaTableIntegerKey(lua_State* state, const char* tableName, const char* keyName)
{
	if (!state || !tableName || !*tableName || !keyName || !*keyName) {
		return false;
	}

	lua_getglobal(state, tableName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return false;
	}

	const int tableIndex = GetAbsoluteLuaIndex(state, -1);
	lua_getfield(state, tableIndex, keyName);
	const bool found = lua_isnumber(state, -1);
	lua_pop(state, 2);
	return found;
}

void SetLuaTableIntegerKey(lua_State* state, const char* tableName, const char* keyName, int value)
{
	if (!state || !tableName || !*tableName || !keyName || !*keyName) {
		return;
	}

	lua_getglobal(state, tableName);
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	const int tableIndex = GetAbsoluteLuaIndex(state, -1);
	lua_pushstring(state, keyName);
	lua_pushinteger(state, value);
	lua_settable(state, tableIndex);
	lua_pop(state, 1);
	DbgLog("[Lua] Synthesizing missing table key %s.%s=%d before loading skill-effect info.\n",
		tableName,
		keyName,
		value);
	}

bool IsUppercaseEnumToken(const std::string& token)

{
	if (token.size() < 3) {
		return false;
	}

	bool hasUnderscore = false;
	for (char ch : token) {
		const unsigned char uch = static_cast<unsigned char>(ch);
		if (ch == '_') {
			hasUnderscore = true;
			continue;
		}
		if ((uch >= 'A' && uch <= 'Z') || (uch >= '0' && uch <= '9')) {
			continue;
		}
		return false;
	}

	return hasUnderscore;
}

bool ShouldTreatAsSkillEnumToken(const std::string& token)
{
	if (!IsUppercaseEnumToken(token)) {
		return false;
	}
	if (_stricmp(token.c_str(), "SKILL_EFFECT_INFO_LIST") == 0
		|| _stricmp(token.c_str(), "SKID") == 0
		|| _stricmp(token.c_str(), "ACTOR_STATE") == 0
		|| _stricmp(token.c_str(), "EFID") == 0
		|| _stricmp(token.c_str(), "LaunchZC_USE_SKILL") == 0) {
		return false;
	}
	return std::strncmp(token.c_str(), "EF_", 3) != 0
		&& std::strncmp(token.c_str(), "ST_", 3) != 0;
}

void PopulateSkillEffectCompatibilityTables(lua_State* state, const std::vector<unsigned char>& bytes)
{
	if (!state || bytes.empty()) {
		return;
	}

	std::unordered_set<std::string> seenTokens;
	std::string token;
	auto flushToken = [&]() {
		if (token.empty()) {
			return;
		}

		if (seenTokens.insert(token).second) {
			if (std::strncmp(token.c_str(), "EF_", 3) == 0) {
				if (!HasLuaTableIntegerKey(state, "EFID", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("EFID", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "EFID", token.c_str(), value);
					}
				}
			} else if (std::strncmp(token.c_str(), "ST_", 3) == 0) {
				if (!HasLuaTableIntegerKey(state, "ACTOR_STATE", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("ACTOR_STATE", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "ACTOR_STATE", token.c_str(), value);
					}
				}
			} else if (ShouldTreatAsSkillEnumToken(token)) {
				if (!HasLuaTableIntegerKey(state, "SKID", token.c_str())) {
					const int value = AllocateSyntheticLuaTableValue("SKID", token.c_str());
					if (value != 0) {
						SetLuaTableIntegerKey(state, "SKID", token.c_str(), value);
					}
				}
			}
		}

		token.clear();
	};

	for (unsigned char byte : bytes) {
		const char ch = static_cast<char>(byte);
		const bool isTokenChar = (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '_';
		if (isTokenChar) {
			token.push_back(ch);
		} else {
			flushToken();
		}
	}
	flushToken();
}

void MergeGeneratedJobCompatibility(lua_State* state);
void AddPcJobVariantAliases(lua_State* state);
void SetJobTblEntry(lua_State* state, const char* nameKey, int value);

void PopulateJobEnumCompatibilityGlobals(lua_State* state, const std::vector<unsigned char>& bytes)
{
	if (!state || bytes.empty()) {
		return;
	}

	std::unordered_set<std::string> seenTokens;
	int synthesizedCount = 0;
	std::string token;
	auto flushToken = [&]() {
		if (token.empty()) {
			return;
		}

		if (seenTokens.insert(token).second && std::strncmp(token.c_str(), "JT_", 3) == 0) {
			int existingValue = 0;
			if (!TryGetLuaJobEnumValue(state, token.c_str(), &existingValue)) {
				int value = 0;
				if (!TryGetGeneratedJobIdFromGlobalName(token.c_str(), &value)) {
					value = AllocateSyntheticLuaTableValue("JT_ENUM", token.c_str());
				}
				if (value != 0) {
					++synthesizedCount;
					SetLuaGlobalInteger(state, token.c_str(), value);
					SetJobTblEntry(state, token.c_str(), value);
					LOG_LUA_SYNTHETIC_GLOBAL("[Lua] Synthesizing missing global %s=%d before loading job-name data.\n",
						token.c_str(),
						value);
				}
			} else {
				SetLuaGlobalInteger(state, token.c_str(), existingValue);
				SetJobTblEntry(state, token.c_str(), existingValue);
			}
		}

		token.clear();
	};

	for (unsigned char byte : bytes) {
		const char ch = static_cast<char>(byte);
		const bool isTokenChar = (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9')
			|| ch == '_';
		if (isTokenChar) {
			token.push_back(ch);
		} else {
			flushToken();
		}
	}
	flushToken();

	if (synthesizedCount > 0) {
		DbgLog("[Lua] Synthesized %d missing JT_* globals before loading job-name data.\n", synthesizedCount);
	}
}

void PrepareGeneratedJobEnumCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);
}

void EnsurePcJobTbl2Table(lua_State* state)
{
	lua_getglobal(state, "pcJobTbl2");
	if (lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pop(state, 1);
	lua_newtable(state);
	lua_setglobal(state, "pcJobTbl2");
}

void SetPcJobTbl2Entry(lua_State* state, const char* nameKey, int value)
{
	if (!state || !nameKey || !*nameKey) {
		return;
	}

	EnsurePcJobTbl2Table(state);
	lua_getglobal(state, "pcJobTbl2");
	lua_pushstring(state, nameKey);
	lua_pushinteger(state, value);
	lua_settable(state, -3);
	lua_pop(state, 1);
}

void EnsureJobTblTable(lua_State* state)
{
	lua_getglobal(state, "jobtbl");
	if (lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pop(state, 1);
	lua_newtable(state);
	lua_setglobal(state, "jobtbl");
}

void SetJobTblEntry(lua_State* state, const char* nameKey, int value)
{
	if (!state || !nameKey || !*nameKey) {
		return;
	}

	EnsureJobTblTable(state);
	lua_getglobal(state, "jobtbl");
	lua_pushstring(state, nameKey);
	lua_pushinteger(state, value);
	lua_settable(state, -3);
	lua_pop(state, 1);
}

void MergePcJobIdentityCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	EnsurePcJobTbl2Table(state);
	lua_getglobal(state, "pcJobTbl");
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_isstring(state, -2) && lua_isnumber(state, -1)) {
			const char* key = lua_tostring(state, -2);
			const int value = static_cast<int>(lua_tointeger(state, -1));

			SetLuaGlobalInteger(state, key, value);
			SetPcJobTbl2Entry(state, key, value);
		}

		lua_pop(state, 1);
	}

	lua_pop(state, 1);
}

void MergeGeneratedJobCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	for (const JobNameEntry& entry : kGeneratedJobNames) {
		std::string globalName = "JT_";
		globalName += entry.name;

		// Archive identities take precedence over compiled compatibility defaults.
		int value = entry.job;
		TryGetLuaJobEnumValue(state, globalName.c_str(), &value);
		SetLuaGlobalInteger(state, globalName.c_str(), value);

		SetJobTblEntry(state, globalName.c_str(), value);
		SetPcJobTbl2Entry(state, globalName.c_str(), value);
	}
}

void AddPcJobVariantAliases(lua_State* state)
{
	if (!state) {
		return;
	}

	struct VariantAlias {
		const char* aliasName;
		const char* baseName;
	};

	static const VariantAlias kAliases[] = {
		{ "JT_RUNE_CHICKEN", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_ROYAL_CHICKEN", "JT_ROYAL_GUARD" },
		{ "JT_ROYAL_CHICKEN_H", "JT_ROYAL_GUARD_H" },
		{ "JT_WOLF_RANGER", "JT_RANGER" },
		{ "JT_WOLF_RANGER_H", "JT_RANGER_H" },
		{ "JT_MADOGEAR", "JT_MECHANIC" },
		{ "JT_MADOGEAR_H", "JT_MECHANIC_H" },
		{ "JT_RUNE_CHICKEN2", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN2_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN3", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN3_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN4", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN4_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN5", "JT_RUNE_KNIGHT" },
		{ "JT_RUNE_CHICKEN5_H", "JT_RUNE_KNIGHT_H" },
		{ "JT_RUNE_CHICKEN_B", "JT_RUNE_KNIGHT_B" },
		{ "JT_ROYAL_CHICKEN_B", "JT_ROYAL_GUARD_B" },
		{ "JT_WOLF_RANGER_B", "JT_RANGER_B" },
		{ "JT_MADOGEAR_B", "JT_MECHANIC_B" },
	};

	for (const VariantAlias& alias : kAliases) {
		int baseValue = 0;
		if (!TryGetLuaGlobalInteger(state, alias.baseName, &baseValue)) {
			continue;
		}

		SetLuaGlobalInteger(state, alias.aliasName, baseValue);
		SetJobTblEntry(state, alias.aliasName, baseValue);
		SetPcJobTbl2Entry(state, alias.aliasName, baseValue);
	}
}

void PrepareJobNameCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);

	lua_getglobal(state, "pcJobTbl");
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		return;
	}

	lua_pushnil(state);
	while (lua_next(state, -2) != 0) {
		if (lua_isstring(state, -2) && lua_isnumber(state, -1)) {
			const char* key = lua_tostring(state, -2);
			const int value = static_cast<int>(lua_tointeger(state, -1));
			SetJobTblEntry(state, key, value);
		}

		lua_pop(state, 1);
	}

	lua_pop(state, 1);
}

void PreparePcJobNameGenderCompatibility(lua_State* state)
{
	if (!state) {
		return;
	}

	MergePcJobIdentityCompatibility(state);
	MergeGeneratedJobCompatibility(state);
	AddPcJobVariantAliases(state);
}

void EnsureSkillIdTable(lua_State* state)
{
	if (!state) {
		return;
	}

	lua_getglobal(state, "SKID");
	if (!lua_istable(state, -1)) {
		lua_pop(state, 1);
		lua_newtable(state);
	}

	for (const SkillEnumEntry& entry : kSkillEnumEntries) {
		if (!entry.name || !*entry.name) {
			continue;
		}
		lua_getfield(state, -1, entry.name);
		const bool hasMapping = lua_isnumber(state, -1);
		lua_pop(state, 1);
		if (!hasMapping) {
			lua_pushinteger(state, entry.id);
			lua_setfield(state, -2, entry.name);
		}
	}
	lua_setglobal(state, "SKID");
}

bool ReadLocalFileBytes(const std::filesystem::path& path, std::vector<unsigned char>* outBytes)
{
	if (!outBytes) {
		return false;
	}

	std::error_code error;
	if (!std::filesystem::exists(path, error) || error) {
		return false;
	}

	FILE* file = std::fopen(path.string().c_str(), "rb");
	if (!file) {
		return false;
	}

	if (std::fseek(file, 0, SEEK_END) != 0) {
		std::fclose(file);
		return false;
	}

	const long size = std::ftell(file);
	if (size < 0 || std::fseek(file, 0, SEEK_SET) != 0) {
		std::fclose(file);
		return false;
	}

	outBytes->resize(static_cast<size_t>(size));
	const size_t bytesRead = std::fread(outBytes->data(), 1, outBytes->size(), file);
	std::fclose(file);
	if (bytesRead != outBytes->size()) {
		outBytes->clear();
		return false;
	}

	return true;
}

} // namespace

CLuaBridge g_buabridge;

CLuaBridge::CLuaBridge()
	: m_state(nullptr)
{
}

CLuaBridge::~CLuaBridge()
{
	Shutdown();
}

bool CLuaBridge::Initialize()
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_state) {
		return true;
	}

	m_state = luaL_newstate();
	if (!m_state) {
		m_lastError = "luaL_newstate failed";
		DbgLog("[Lua] Failed to create Lua 5.1 state.\n");
		return false;
	}

	luaL_openlibs(m_state);
	m_lastError.clear();
	DbgLog("[Lua] Initialized Lua 5.1 runtime.\n");
	return true;
}

void CLuaBridge::Shutdown()
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_state) {
		lua_close(m_state);
		m_state = nullptr;
	}
	m_loadedScripts.clear();
	m_skillEffectInfoCache.clear();
	m_missingSkillEffectInfoIds.clear();
	m_statusIconScriptsAttempted = false;
	m_statusIconOverrides.clear();
	m_statusIconInfoCache.clear();
	m_lastError.clear();
}

bool CLuaBridge::IsInitialized() const
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);
	return m_state != nullptr;
}

std::string CLuaBridge::NormalizePath(const char* path)
{
	if (!path || !*path) {
		return std::string();
	}

	return NormalizeSlashPath(path);
}

std::string CLuaBridge::ResolveLocalLuaPath(const char* relativePath) const
{
	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		return std::string();
	}

	std::error_code error;
	const std::filesystem::path current = std::filesystem::current_path(error);
	if (error) {
		return std::string();
	}

	// Check every loose-script alias before searching either GRF.
	const std::string candidates[] = {
		normalized,
		std::string("data\\luafiles514\\") + normalized,
		std::string("data\\") + normalized,
	};
	for (const std::string& candidate : candidates) {
		if (!CFileMgr::CanReadLooseFile(candidate.c_str())) {
			continue;
		}
		std::string nativePath = candidate;
		std::replace(nativePath.begin(), nativePath.end(), '\\', '/');
		const std::filesystem::path local = current / nativePath;
		if (std::filesystem::is_regular_file(local, error) && !error) {
			return local.string();
		}
	}

	return std::string();
}

bool CLuaBridge::ReadRagnarokScriptBytes(const char* relativePath,
	std::vector<unsigned char>* outBytes,
	std::string* outSourcePath,
	bool* outLoadedFromLocal) const
{
	if (!outBytes || !outSourcePath || !outLoadedFromLocal) {
		return false;
	}

	outBytes->clear();
	outSourcePath->clear();
	*outLoadedFromLocal = false;

	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		return false;
	}

	const std::string localPath = ResolveLocalLuaPath(normalized.c_str());
	if (!localPath.empty() && ReadLocalFileBytes(std::filesystem::path(localPath), outBytes)) {
		*outSourcePath = localPath;
		*outLoadedFromLocal = true;
		return true;
	}

	std::vector<std::string> fallbackCandidates;
	fallbackCandidates.push_back(normalized);

	const char* const lua514Prefix = "data\\luafiles514\\";
	if (normalized.size() > std::strlen(lua514Prefix)
		&& _strnicmp(normalized.c_str(), lua514Prefix, std::strlen(lua514Prefix)) == 0) {
		fallbackCandidates.push_back(normalized.substr(std::strlen(lua514Prefix)));
	} else {
		fallbackCandidates.push_back(std::string(lua514Prefix) + normalized);
	}

	if (_strnicmp(normalized.c_str(), "data\\", 5) == 0) {
		fallbackCandidates.push_back(normalized.substr(5));
	} else {
		fallbackCandidates.push_back(std::string("data\\") + normalized);
	}

	for (const std::string& candidate : fallbackCandidates) {
		int size = 0;
		unsigned char* data = g_fileMgr.GetData(candidate.c_str(), &size);
		if (!data || size <= 0) {
			delete[] data;
			continue;
		}

		outBytes->assign(data, data + size);
		delete[] data;
		*outSourcePath = candidate;
		*outLoadedFromLocal = false;
		return true;
	}

	return false;
}

bool CLuaBridge::ExecuteBuffer(const unsigned char* bytes, size_t size, const char* chunkName, const char* returnedTableGlobalName)
{
	if (!Initialize()) {
		return false;
	}
	if (!bytes || size == 0) {
		m_lastError = "empty Lua buffer";
		return false;
	}

	const int loadResult = luaL_loadbuffer(
		m_state,
		reinterpret_cast<const char*>(bytes),
		size,
		chunkName ? chunkName : "=(buffer)");
	if (loadResult != 0) {
		const char* errorText = lua_tostring(m_state, -1);
		m_lastError = errorText ? errorText : "luaL_loadbuffer failed";
		DbgLog("[Lua] Load failed for '%s': %s\n", chunkName ? chunkName : "(buffer)", m_lastError.c_str());
		lua_settop(m_state, 0);
		return false;
	}

	const int callResult = lua_pcall(m_state, 0, LUA_MULTRET, 0);
	if (callResult != 0) {
		const char* errorText = lua_tostring(m_state, -1);
		m_lastError = errorText ? errorText : "lua_pcall failed";
		DbgLog("[Lua] Execution failed for '%s': %s\n", chunkName ? chunkName : "(buffer)", m_lastError.c_str());
		lua_settop(m_state, 0);
		return false;
	}

	if (returnedTableGlobalName && *returnedTableGlobalName) {
		lua_getglobal(m_state, returnedTableGlobalName);
		const bool alreadyBound = lua_istable(m_state, -1);
		lua_pop(m_state, 1);

		if (!alreadyBound) {
			// Item-info scripts commonly assign tbl instead of returning it.
			if (std::strcmp(returnedTableGlobalName, "OpenMidgardItemInfo") == 0) {
				lua_getglobal(m_state, "tbl");
				if (lua_istable(m_state, -1)) {
					lua_setglobal(m_state, returnedTableGlobalName);
				} else {
					lua_pop(m_state, 1);
				}
			}

			const int resultCount = lua_gettop(m_state);
			for (int index = 1; index <= resultCount; ++index) {
				if (!lua_istable(m_state, index)) {
					continue;
				}

				lua_pushvalue(m_state, index);
				lua_setglobal(m_state, returnedTableGlobalName);
				DbgLog("[Lua] Bound returned table to global '%s' after executing '%s'.\n",
					returnedTableGlobalName,
					chunkName ? chunkName : "(buffer)");
				break;
			}
		}
	}

	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::LoadRagnarokScript(const char* relativePath)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const std::string normalized = NormalizePath(relativePath);
	if (!Initialize()) {
		return false;
	}

	if (IsAccessoryNameTableScript(normalized)) {
		LoadRagnarokScriptOnce("lua files\\datainfo\\accessoryid.lub");
	}
	if (PathEndsWithInsensitive(normalized, "skillinfoz\\skillinfolist.lub")
		|| PathEndsWithInsensitive(normalized, "skillinfoz\\skillinfolist.lua")
		|| PathEndsWithInsensitive(normalized, "skillinfoz\\skilldescript.lub")
		|| PathEndsWithInsensitive(normalized, "skillinfoz\\skilldescript.lua")) {
		// Custom ID tables may omit legacy aliases still used by their metadata.
		// Keep their explicit mappings and fill only missing known names.
		EnsureSkillIdTable(m_state);
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobidentity_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\npcidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\npcidentity_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcjobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		PrepareGeneratedJobEnumCompatibility(m_state);
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")) {
		// JobNameTable is indexed by the real IDs from this archive's jobtbl.
		LoadRagnarokScriptOnce("lua files\\datainfo\\npcidentity.lub");
		PrepareJobNameCompatibility(m_state);
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		LoadRagnarokScriptOnce("lua files\\admin\\pcidentity.lub");
		PreparePcJobNameGenderCompatibility(m_state);
	}

	std::vector<unsigned char> bytes;
	std::string sourcePath;
	bool loadedFromLocal = false;
	if (!ReadRagnarokScriptBytes(relativePath, &bytes, &sourcePath, &loadedFromLocal)) {
		m_lastError = "unable to resolve Lua/LUB script";
		DbgLog("[Lua] Failed to resolve script '%s'.\n", relativePath ? relativePath : "(null)");
		return false;
	}

	if (PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobname_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcjobname.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\jobidentity_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\npcidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\npcidentity_f.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\admin\\pcidentity.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender.lub")
		|| PathEndsWithInsensitive(normalized, "lua files\\datainfo\\pcjobnamegender_f.lub")) {
		PopulateJobEnumCompatibilityGlobals(m_state, bytes);
	}

	const bool executed = ExecuteBuffer(
		bytes.data(),
		bytes.size(),
		sourcePath.c_str(),
		GetReturnedTableBindingName(normalized));
	if (executed) {
		DbgLog("[Lua] Loaded %s script: %s\n", loadedFromLocal ? "local" : "fallback", sourcePath.c_str());
	}
	return executed;
}

bool CLuaBridge::LoadRagnarokScriptOnce(const char* relativePath)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const std::string normalized = NormalizePath(relativePath);
	if (normalized.empty()) {
		m_lastError = "empty Lua path";
		return false;
	}

	if (HasLoadedScript(normalized.c_str())) {
		return true;
	}

	if (!LoadRagnarokScript(normalized.c_str())) {
		return false;
	}

	m_loadedScripts.push_back(normalized);
	return true;
}

bool CLuaBridge::HasLoadedScript(const char* relativePath) const
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	const std::string normalized = NormalizePath(relativePath);
	return std::find(m_loadedScripts.begin(), m_loadedScripts.end(), normalized) != m_loadedScripts.end();
}

bool CLuaBridge::GetGlobalTableIntegerByIntegerKey(const char* tableName, int numericKey, int* outValue)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!outValue) {
		return false;
	}
	*outValue = 0;

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_isnumber(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a number";
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(m_state, -1));
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableIntegerByStringKey(const char* tableName, const char* stringKey, int* outValue)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!outValue) {
		return false;
	}
	*outValue = 0;

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!stringKey || !*stringKey) {
		m_lastError = "empty Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushstring(m_state, stringKey);
	lua_gettable(m_state, -2);
	if (!lua_isnumber(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a number";
		return false;
	}

	*outValue = static_cast<int>(lua_tointeger(m_state, -1));
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableNestedStringByIntegerKey(const char* tableName,
	int numericKey,
	const char* nestedStringKey,
	std::string* outValue)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!nestedStringKey || !*nestedStringKey) {
		m_lastError = "empty nested Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua nested table not found";
		return false;
	}

	lua_pushstring(m_state, nestedStringKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua nested table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetItemInfoByItemId(unsigned int itemId, ItemMetadata* outInfo)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!outInfo || itemId == 0 || !Initialize()) {
		return false;
	}
	*outInfo = ItemMetadata{};

	lua_getglobal(m_state, "OpenMidgardItemInfo");
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		return false;
	}
	lua_pushnumber(m_state, static_cast<lua_Number>(itemId));
	lua_gettable(m_state, -2);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		return false;
	}

	const int entryIndex = lua_gettop(m_state);
	const auto readString = [&](const char* key, std::string& out) {
		lua_getfield(m_state, entryIndex, key);
		if (lua_isstring(m_state, -1)) {
			size_t length = 0;
			const char* value = lua_tolstring(m_state, -1, &length);
			out.assign(value, length);
		}
		lua_pop(m_state, 1);
	};
	readString("unidentifiedDisplayName", outInfo->unidentifiedDisplayName);
	readString("identifiedDisplayName", outInfo->identifiedDisplayName);
	readString("unidentifiedResourceName", outInfo->unidentifiedResourceName);
	readString("identifiedResourceName", outInfo->identifiedResourceName);

	lua_getfield(m_state, entryIndex, "slotCount");
	if (lua_isnumber(m_state, -1)) {
		outInfo->slotCount = (std::max)(0, static_cast<int>(lua_tointeger(m_state, -1)));
	}
	lua_pop(m_state, 1);

	lua_getfield(m_state, entryIndex, "ClassNum");
	if (lua_type(m_state, -1) == LUA_TNUMBER) {
		const lua_Number classNum = lua_tonumber(m_state, -1);
		if (std::isfinite(classNum) && classNum >= 0 && classNum <= 65535
			&& classNum == std::floor(classNum)) {
			outInfo->classNum = static_cast<int>(classNum);
		}
	}
	lua_pop(m_state, 1);

	const auto readDescription = [&](const char* key) {
		lua_getfield(m_state, entryIndex, key);
		if (lua_istable(m_state, -1)) {
			const size_t count = lua_objlen(m_state, -1);
			for (size_t index = 1; index <= count; ++index) {
				lua_rawgeti(m_state, -1, static_cast<int>(index));
				if (lua_isstring(m_state, -1)) {
					if (index > 1) {
						outInfo->description += '\n';
					}
					size_t length = 0;
					const char* value = lua_tolstring(m_state, -1, &length);
					outInfo->description.append(value, length);
				}
				lua_pop(m_state, 1);
			}
		}
		lua_pop(m_state, 1);
	};
	readDescription("identifiedDescriptionName");
	if (outInfo->description.empty()) {
		readDescription("unidentifiedDescriptionName");
	}

	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetSkillMetadata(std::vector<SkillMetadata>* outSkills)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!outSkills || !Initialize()) {
		return false;
	}
	outSkills->clear();
	std::unordered_map<int, SkillMetadata> entries;

	lua_getglobal(m_state, "SKID");
	if (lua_istable(m_state, -1)) {
		lua_pushnil(m_state);
		while (lua_next(m_state, -2) != 0) {
			if (lua_type(m_state, -2) == LUA_TSTRING && lua_isnumber(m_state, -1)) {
				const int id = static_cast<int>(lua_tointeger(m_state, -1));
				if (id > 0) {
					SkillMetadata& metadata = entries[id];
					metadata.skillId = id;
					metadata.skillIdName = lua_tostring(m_state, -2);
				}
			}
			lua_pop(m_state, 1);
		}
	}
	lua_pop(m_state, 1);

	lua_getglobal(m_state, "SKILL_INFO_LIST");
	if (lua_istable(m_state, -1)) {
		lua_pushnil(m_state);
		while (lua_next(m_state, -2) != 0) {
			if (lua_isnumber(m_state, -2) && lua_istable(m_state, -1)) {
				const int id = static_cast<int>(lua_tointeger(m_state, -2));
				if (id > 0) {
					SkillMetadata& metadata = entries[id];
					metadata.skillId = id;
					const int entryIndex = lua_gettop(m_state);
					lua_rawgeti(m_state, entryIndex, 1);
					if (lua_isstring(m_state, -1)) {
						metadata.skillIdName = lua_tostring(m_state, -1);
					}
					lua_pop(m_state, 1);
					lua_getfield(m_state, entryIndex, "SkillName");
					if (lua_isstring(m_state, -1)) {
						metadata.displayName = lua_tostring(m_state, -1);
					}
					lua_pop(m_state, 1);
					TryGetLuaTableIntegerField(m_state, entryIndex, "MaxLv", &metadata.maxLevel);
					TryGetLuaTableIntegerArrayField(m_state, entryIndex, "SpAmount", &metadata.levelSpCosts);
				}
			}
			lua_pop(m_state, 1);
		}
	}
	lua_pop(m_state, 1);

	lua_getglobal(m_state, "SKILL_DESCRIPT");
	if (lua_istable(m_state, -1)) {
		lua_pushnil(m_state);
		while (lua_next(m_state, -2) != 0) {
			if (lua_isnumber(m_state, -2) && lua_istable(m_state, -1)) {
				const int id = static_cast<int>(lua_tointeger(m_state, -2));
				if (id > 0) {
					SkillMetadata& metadata = entries[id];
					metadata.skillId = id;
					const int entryIndex = lua_gettop(m_state);
					const size_t count = lua_objlen(m_state, entryIndex);
					for (size_t index = 1; index <= count; ++index) {
						lua_rawgeti(m_state, entryIndex, static_cast<int>(index));
						if (lua_isstring(m_state, -1)) {
							if (index == 1) {
								if (metadata.displayName.empty()) {
									metadata.displayName = lua_tostring(m_state, -1);
								}
							} else {
								metadata.descriptionLines.emplace_back(lua_tostring(m_state, -1));
							}
						}
						lua_pop(m_state, 1);
					}
				}
			}
			lua_pop(m_state, 1);
		}
	}
	lua_pop(m_state, 1);

	for (auto& entry : entries) {
		outSkills->push_back(std::move(entry.second));
	}
	m_lastError.clear();
	return !outSkills->empty();
}

bool CLuaBridge::GetGlobalTableStringByIntegerKey(const char* tableName, int numericKey, std::string* outValue)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushinteger(m_state, numericKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetGlobalTableStringByStringKey(const char* tableName, const char* stringKey, std::string* outValue)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (!outValue) {
		return false;
	}
	outValue->clear();

	if (!Initialize()) {
		return false;
	}
	if (!tableName || !*tableName) {
		m_lastError = "empty Lua table name";
		return false;
	}
	if (!stringKey || !*stringKey) {
		m_lastError = "empty Lua string key";
		return false;
	}

	lua_getglobal(m_state, tableName);
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table not found";
		return false;
	}

	lua_pushstring(m_state, stringKey);
	lua_gettable(m_state, -2);
	if (!lua_isstring(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "Lua table value is not a string";
		return false;
	}

	const char* value = lua_tostring(m_state, -1);
	if (!value) {
		lua_settop(m_state, 0);
		m_lastError = "Lua string conversion failed";
		return false;
	}

	outValue->assign(value);
	lua_settop(m_state, 0);
	m_lastError.clear();
	return true;
}

bool CLuaBridge::GetSkillEffectInfoBySkillId(int skillId, LuaSkillEffectInfo* outInfo)
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (outInfo) {
		*outInfo = LuaSkillEffectInfo{};
	}
	if (skillId <= 0) {
		return false;
	}

	const auto cacheIt = m_skillEffectInfoCache.find(skillId);
	if (cacheIt != m_skillEffectInfoCache.end()) {
		if (outInfo) {
			*outInfo = cacheIt->second;
		}
		return true;
	}
	if (m_missingSkillEffectInfoIds.find(skillId) != m_missingSkillEffectInfoIds.end()) {
		return false;
	}

	if (!Initialize()) {
		return false;
	}

	EnsureSkillIdTable(m_state);

	if (!LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\actorstate.lub")
		|| !LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\skilleffectinfo_f.lub")
		|| !LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\EffectID.lub")) {
		return false;
	}

	std::vector<unsigned char> skillEffectListBytes;
	std::string skillEffectListSourcePath;
	bool skillEffectListLoadedFromLocal = false;
	if (ReadRagnarokScriptBytes("lua files\\skilleffectinfo\\SkillEffectInfoList.lub",
		&skillEffectListBytes,
		&skillEffectListSourcePath,
		&skillEffectListLoadedFromLocal)) {
		PopulateSkillEffectCompatibilityTables(m_state, skillEffectListBytes);
	}

	if (!LoadRagnarokScriptOnce("lua files\\skilleffectinfo\\SkillEffectInfoList.lub")) {
		return false;
	}

	lua_getglobal(m_state, "SKILL_EFFECT_INFO_LIST");
	if (!lua_istable(m_state, -1)) {
		lua_settop(m_state, 0);
		m_lastError = "SKILL_EFFECT_INFO_LIST not found";
		return false;
	}

	LuaSkillEffectInfo info;
	bool found = false;
	auto readEntry = [&]() {
		info.hasBeginEffectId = TryGetLuaTableIntegerField(m_state, -1, "beginEffectID", &info.beginEffectId);
		info.hasBeginMotionType = TryGetLuaTableIntegerField(m_state, -1, "beginMotionType", &info.beginMotionType);
		info.hasTargetEffectId = TryGetLuaTableIntegerField(m_state, -1, "targetEffectID", &info.targetEffectId);
		if (!TryGetLuaTableIntegerArrayField(m_state, -1, "targetEffectID", &info.targetEffectIds) && info.hasTargetEffectId) {
			info.targetEffectIds.push_back(info.targetEffectId);
		}
		if (!info.targetEffectIds.empty()) {
			info.hasTargetEffectId = true;
			info.targetEffectId = info.targetEffectIds.front();
		}
		info.hasGroundEffectId = TryGetLuaTableIntegerField(m_state, -1, "groundEffectID", &info.groundEffectId);
		if (!TryGetLuaTableIntegerArrayField(m_state, -1, "groundEffectID", &info.groundEffectIds) && info.hasGroundEffectId) {
			info.groundEffectIds.push_back(info.groundEffectId);
		}
		if (!info.groundEffectIds.empty()) {
			info.hasGroundEffectId = true;
			info.groundEffectId = info.groundEffectIds.front();
		}
		if (!TryGetLuaTableIntegerArrayField(m_state, -1, "effectID", &info.effectIds)) {
			int effectId = -1;
			if (TryGetLuaTableIntegerField(m_state, -1, "effectID", &effectId)) {
				info.effectIds.push_back(effectId);
			} else {
				TryGetLuaTableIntegerArrayField(m_state, -1, "effectIDs", &info.effectIds);
			}
		}
		info.hasOnTarget = TryGetLuaTableBooleanField(m_state, -1, "onTarget", &info.onTarget);
		info.hasLaunchUseSkill = TryGetLuaTableBooleanField(m_state, -1, "LaunchZC_USE_SKILL", &info.launchUseSkill);
		found = true;
	};

	// Current clients key this table by SKID; older custom lists store SKID in each entry.
	lua_rawgeti(m_state, -1, skillId);
	int embeddedSkillId = 0;
	if (lua_istable(m_state, -1)
		&& (!TryGetLuaTableIntegerField(m_state, -1, "SKID", &embeddedSkillId) || embeddedSkillId == skillId)) {
		readEntry();
	}
	lua_pop(m_state, 1);
	if (!found) {
		lua_pushnil(m_state);
		while (lua_next(m_state, -2) != 0) {
			int entrySkillId = 0;
			if (lua_istable(m_state, -1)
				&& TryGetLuaTableIntegerField(m_state, -1, "SKID", &entrySkillId) && entrySkillId == skillId) {
				readEntry();
				lua_pop(m_state, 1);
				break;
			}
			lua_pop(m_state, 1);
		}
	}

	lua_settop(m_state, 0);
	if (!found) {
		m_missingSkillEffectInfoIds.insert(skillId);
		return false;
	}

	m_skillEffectInfoCache.emplace(skillId, info);
	if (outInfo) {
		*outInfo = info;
	}
	return true;
}

void CLuaBridge::LoadStatusIconOverrides()
{
	std::vector<std::filesystem::path> candidates = {
		GetOpenMidgardIniPath().parent_path() / "status_icons.lua",
	};
	std::error_code error;
	const auto current = std::filesystem::current_path(error);
	if (!error) candidates.push_back(current / "status_icons.lua");
#ifdef RO_SOURCE_ROOT
	candidates.emplace_back(std::filesystem::path(RO_SOURCE_ROOT) / "status_icons.lua");
#endif
	std::filesystem::path source;
	for (const auto& candidate : candidates) {
		if (std::filesystem::is_regular_file(candidate, error) && !error) {
			source = candidate;
			break;
		}
	}
	if (source.empty()) {
		DbgLog("[StatusIcon] status_icons.lua not found; using automatic lookup.\n");
		return;
	}
	const std::string path = source.string();
	lua_settop(m_state, 0);
	if (luaL_loadfile(m_state, path.c_str()) != 0 || lua_pcall(m_state, 0, 1, 0) != 0) {
		const char* message = lua_tostring(m_state, -1);
		DbgLog("[StatusIcon] rejected '%s': %s\n", path.c_str(), message ? message : "could not load table");
		lua_settop(m_state, 0);
		return;
	}
	int version = 0;
	if (!lua_istable(m_state, -1)
		|| !TryGetLuaTableIntegerField(m_state, -1, "version", &version) || version != 1) {
		DbgLog("[StatusIcon] rejected '%s': expected a table with version = 1.\n", path.c_str());
		lua_settop(m_state, 0);
		return;
	}
	lua_getfield(m_state, -1, "icons");
	if (!lua_istable(m_state, -1)) {
		DbgLog("[StatusIcon] rejected '%s': missing icons table.\n", path.c_str());
		lua_settop(m_state, 0);
		return;
	}
	lua_pushnil(m_state);
	while (lua_next(m_state, -2) != 0) {
		const double status = lua_tonumber(m_state, -2);
		if (lua_type(m_state, -2) != LUA_TNUMBER || !(status >= 0 && status <= 65535)
			|| status != std::floor(status) || !lua_istable(m_state, -1)) {
			DbgLog("[StatusIcon] ignored invalid entry in '%s': expected a numeric status ID and table.\n", path.c_str());
			lua_pop(m_state, 1);
			continue;
		}
		LuaStatusIconInfo info;
		lua_getfield(m_state, -1, "name");
		if (lua_type(m_state, -1) == LUA_TSTRING) info.displayName = lua_tostring(m_state, -1);
		lua_pop(m_state, 1);
		lua_getfield(m_state, -1, "icon");
		if (lua_type(m_state, -1) == LUA_TSTRING) {
			info.iconGrfPath = NormalizeSlashPath(lua_tostring(m_state, -1));
		}
		lua_pop(m_state, 1);
		if (!info.displayName.empty() || !info.iconGrfPath.empty()) {
			m_statusIconOverrides.emplace(static_cast<int>(status), std::move(info));
		} else {
			DbgLog("[StatusIcon] ignored status %d in '%s': expected name or icon.\n", static_cast<int>(status), path.c_str());
		}
		lua_pop(m_state, 1);
	}
	lua_settop(m_state, 0);
	DbgLog("[StatusIcon] loaded '%s': %zu overrides.\n", path.c_str(), m_statusIconOverrides.size());
}

bool CLuaBridge::GetStatusIconInfoByStatusType(int statusType, LuaStatusIconInfo* outInfo)
{
	const std::lock_guard<std::recursive_mutex> lock(m_mutex);
	if (!outInfo || statusType < 0) {
		return false;
	}
	*outInfo = LuaStatusIconInfo{};
	if (!Initialize()) {
		return false;
	}
	const auto cached = m_statusIconInfoCache.find(statusType);
	if (cached != m_statusIconInfoCache.end()) {
		*outInfo = cached->second;
		return !outInfo->displayName.empty() || !outInfo->iconFileName.empty() || !outInfo->iconGrfPath.empty();
	}
	if (!m_statusIconScriptsAttempted) {
		m_statusIconScriptsAttempted = true;
		LoadStatusIconOverrides();
		if (LoadRagnarokScriptOnce("lua files\\stateicon\\efstids.lub")) {
			LoadRagnarokScriptOnce("lua files\\stateicon\\stateiconimginfo.lub");
			LoadRagnarokScriptOnce("lua files\\stateicon\\stateiconinfo.lub");
		}
	}

	LuaStatusIconInfo info;
	lua_getglobal(m_state, "StateIconImgList");
	if (lua_istable(m_state, -1)) {
		// Image entries are grouped by priority (gold, red, blue, green, white).
		for (int priority = 0; priority <= 4 && info.iconFileName.empty(); ++priority) {
			lua_rawgeti(m_state, -1, priority);
			if (lua_istable(m_state, -1)) {
				lua_rawgeti(m_state, -1, statusType);
				if (lua_type(m_state, -1) == LUA_TSTRING) {
					info.iconFileName = lua_tostring(m_state, -1);
				}
				lua_pop(m_state, 1);
			}
			lua_pop(m_state, 1);
		}
	}
	lua_settop(m_state, 0);

	lua_getglobal(m_state, "StateIconList");
	if (lua_istable(m_state, -1)) {
		lua_rawgeti(m_state, -1, statusType);
		if (lua_istable(m_state, -1)) {
			lua_getfield(m_state, -1, "descript");
			if (lua_istable(m_state, -1)) {
				lua_rawgeti(m_state, -1, 1);
				if (lua_istable(m_state, -1)) {
					lua_rawgeti(m_state, -1, 1);
					if (lua_type(m_state, -1) == LUA_TSTRING) {
						info.displayName = lua_tostring(m_state, -1);
					}
				}
			}
		}
	}
	lua_settop(m_state, 0);
	const auto overrideInfo = m_statusIconOverrides.find(statusType);
	if (overrideInfo != m_statusIconOverrides.end()) {
		if (!overrideInfo->second.displayName.empty()) info.displayName = overrideInfo->second.displayName;
		info.iconGrfPath = overrideInfo->second.iconGrfPath;
	}
	m_statusIconInfoCache.emplace(statusType, info);
	*outInfo = info;
	return !info.displayName.empty() || !info.iconFileName.empty() || !info.iconGrfPath.empty();
}

const std::string& CLuaBridge::GetLastError() const
{
	return m_lastError;
}
