#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct SkillMetadata {
    int skillId = 0;
    std::string skillIdName;
    std::string displayName;
    int maxLevel = 0;
    std::vector<std::string> descriptionLines;
    std::vector<int> levelSpCosts;
};

class CSkillMgr {
public:
    CSkillMgr();
    ~CSkillMgr();

    bool EnsureLoaded();
    const SkillMetadata* GetSkillMetadata(int skillId) const;
    const SkillMetadata* GetSkillMetadataByName(const std::string& skillIdName) const;
    std::string GetSkillIconPath(int skillId) const;

private:
    bool LoadSkillMetadata();
    bool BuildSkillIdMapFromNameTable();
    void LoadSkillDisplayNames();
    void LoadSkillDescriptions();
    void LoadSkillLevelSpCosts();
    void LoadLuaSkillMetadata();

    std::unordered_map<int, SkillMetadata> m_byId;
    std::unordered_map<std::string, int> m_idBySkillName;
    bool m_loaded;
};

extern CSkillMgr g_skillMgr;
