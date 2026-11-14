#pragma once

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct lua_State;
struct ItemMetadata;
struct SkillMetadata;
struct SkillTreeLayout;

struct LuaStatusIconInfo {
    std::string displayName;
    std::string iconFileName;
    std::string iconGrfPath; // Full path from status_icons.lua; overrides automatic lookup.
};

struct LuaSkillEffectInfo {
    bool hasBeginEffectId = false;
    int beginEffectId = -1;
    bool hasBeginMotionType = false;
    int beginMotionType = -1;
    bool hasTargetEffectId = false;
    int targetEffectId = -1;
    bool hasGroundEffectId = false;
    int groundEffectId = -1;
    std::vector<int> effectIds;
    std::vector<int> targetEffectIds;
    std::vector<int> groundEffectIds;
    bool hasOnTarget = false;
    bool onTarget = false;
    bool hasLaunchUseSkill = false;
    bool launchUseSkill = true;
};

class CLuaBridge
{
public:
    CLuaBridge();
    ~CLuaBridge();

    bool Initialize();
    void Shutdown();
    bool IsInitialized() const;

    bool LoadRagnarokScript(const char* relativePath);
    bool LoadRagnarokScriptOnce(const char* relativePath);
    bool HasLoadedScript(const char* relativePath) const;
    bool GetGlobalTableIntegerByIntegerKey(const char* tableName, int numericKey, int* outValue);
    bool GetGlobalTableIntegerByStringKey(const char* tableName, const char* stringKey, int* outValue);
    bool GetGlobalTableNestedStringByIntegerKey(const char* tableName, int numericKey, const char* nestedStringKey, std::string* outValue);
    bool GetGlobalTableStringByIntegerKey(const char* tableName, int numericKey, std::string* outValue);
    bool GetGlobalTableStringByStringKey(const char* tableName, const char* stringKey, std::string* outValue);
    bool GetItemInfoByItemId(unsigned int itemId, ItemMetadata* outInfo);
    bool GetSkillMetadata(std::vector<SkillMetadata>* outSkills);
    bool GetSkillTreeLayouts(std::vector<SkillTreeLayout>* outLayouts, std::unordered_map<int, int>* outInheritance);
    bool GetSkillEffectInfoBySkillId(int skillId, LuaSkillEffectInfo* outInfo);
    bool GetStatusIconInfoByStatusType(int statusType, LuaStatusIconInfo* outInfo);

    const std::string& GetLastError() const;
    std::string ResolveLocalLuaPath(const char* relativePath) const;

private:
    static std::string NormalizePath(const char* path);
    bool ReadRagnarokScriptBytes(const char* relativePath,
        std::vector<unsigned char>* outBytes,
        std::string* outSourcePath,
        bool* outLoadedFromLocal) const;
    bool ExecuteBuffer(const unsigned char* bytes, size_t size, const char* chunkName, const char* returnedTableGlobalName = nullptr);
    void LoadStatusIconOverrides();

    lua_State* m_state;
    mutable std::recursive_mutex m_mutex;
    std::vector<std::string> m_loadedScripts;
    std::unordered_map<int, LuaSkillEffectInfo> m_skillEffectInfoCache;
    std::unordered_set<int> m_missingSkillEffectInfoIds;
    bool m_statusIconScriptsAttempted = false;
    std::unordered_map<int, LuaStatusIconInfo> m_statusIconOverrides;
    std::unordered_map<int, LuaStatusIconInfo> m_statusIconInfoCache;
    std::string m_lastError;
};

extern CLuaBridge g_buabridge;
