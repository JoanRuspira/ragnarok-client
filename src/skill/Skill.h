#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct SkillMetadata {
    int skillId = 0;
    std::string skillIdName;
    std::string displayName;
    int maxLevel = 0;
    bool separateLevels = false; // bSeperateLv in SKILL_INFO_LIST.
    std::vector<std::string> descriptionLines;
    std::vector<int> levelSpCosts;
};

inline constexpr int kSkillTreeColumns = 5;
inline constexpr int kSkillTreeRows = 6;
inline constexpr int kSkillTreeJobTabs = 2;

struct SkillTreeLayout {
    int jobId = 0;
    std::array<int, kSkillTreeColumns * kSkillTreeRows> skillIds{};
    std::array<std::string, kSkillTreeColumns> titles{};
};

class CSkillMgr {
public:
    CSkillMgr();
    ~CSkillMgr();

    bool EnsureLoaded();
    const SkillMetadata* GetSkillMetadata(int skillId) const;
    const SkillMetadata* GetSkillMetadataByName(const std::string& skillIdName) const;
    std::string GetSkillIconPath(int skillId) const;
    std::string GetSkillTreeIconPath(int skillId) const;
    std::array<SkillTreeLayout, kSkillTreeJobTabs> GetSkillTreeTabs(int jobId);

private:
    bool LoadSkillMetadata();
    bool BuildSkillIdMapFromNameTable();
    void LoadSkillDisplayNames();
    void LoadSkillDescriptions();
    void LoadSkillLevelSpCosts();
    void LoadLuaSkillMetadata();
    void LoadSkillTrees();

    std::unordered_map<int, SkillMetadata> m_byId;
    std::unordered_map<std::string, int> m_idBySkillName;
    bool m_loaded;
    bool m_skillTreesLoaded = false;
    std::unordered_map<int, SkillTreeLayout> m_skillTrees;
    std::unordered_map<int, int> m_jobInheritance;
};

extern CSkillMgr g_skillMgr;
