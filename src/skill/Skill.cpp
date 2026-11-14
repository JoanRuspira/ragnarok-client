// Skill.cpp - Skill data and usage logic
#include "Skill.h"
#include "SkillPresentation.h"

#include "core/File.h"
#include "lua/LuaBridge.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace {

#include "SkillEnumIdTable.inc"

constexpr const char* kUiKorPrefix =
    "texture\\"
    "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA"
    "\\";

std::string TrimAscii(const std::string& value)
{
    const auto isSpace = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };

    size_t begin = 0;
    while (begin < value.size() && isSpace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin && isSpace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

bool ReadLines(const char* fileName, std::vector<std::string>& outLines)
{
    outLines.clear();
    int size = 0;
    unsigned char* data = g_fileMgr.GetData(fileName, &size);
    if (!data || size <= 0) {
        delete[] data;
        const std::string dataPath = std::string("data\\") + fileName;
        data = g_fileMgr.GetData(dataPath.c_str(), &size);
    }
    if (!data || size <= 0) {
        delete[] data;
        return false;
    }
    std::istringstream stream(std::string(reinterpret_cast<const char*>(data), size));
    delete[] data;

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        outLines.push_back(line);
    }

    return true;
}

std::vector<std::string> SplitHashTokens(const std::string& line)
{
    std::vector<std::string> out;
    std::string current;
    for (char ch : line) {
        if (ch == '#') {
            out.push_back(current);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        out.push_back(current);
    }
    return out;
}

} // namespace

CSkillMgr::CSkillMgr()
    : m_loaded(false)
{
}

CSkillMgr::~CSkillMgr() = default;

bool CSkillMgr::EnsureLoaded()
{
    if (m_loaded) {
        return !m_byId.empty();
    }
    m_loaded = true;
    return LoadSkillMetadata();
}

const SkillMetadata* CSkillMgr::GetSkillMetadata(int skillId) const
{
    const auto it = m_byId.find(skillId);
    if (it == m_byId.end()) {
        return nullptr;
    }
    return &it->second;
}

const SkillMetadata* CSkillMgr::GetSkillMetadataByName(const std::string& skillIdName) const
{
    const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
    if (it == m_idBySkillName.end()) {
        return nullptr;
    }
    return GetSkillMetadata(it->second);
}

std::string CSkillMgr::GetSkillIconPath(int skillId) const
{
    const SkillMetadata* metadata = GetSkillMetadata(skillId);
    if (!metadata || metadata->skillIdName.empty()) {
        return std::string();
    }

    return std::string(kUiKorPrefix) + "item\\" + ToLowerAscii(metadata->skillIdName) + ".bmp";
}

std::string CSkillMgr::GetSkillTreeIconPath(int skillId) const
{
    const std::string path = GetSkillIconPath(skillId);
    if (path.empty()) {
        return {};
    }
    // The loose data folder contains the 64px artwork; legacy GRFs may still
    // contain a 24px icon with the same name. Respect the data-folder setting.
    const std::string dataPath = "data\\" + path;
    if (g_fileMgr.IsDataExist(dataPath.c_str())) {
        return dataPath;
    }
    return path;
}

void CSkillMgr::LoadSkillTrees()
{
    if (m_skillTreesLoaded) {
        return;
    }
    EnsureLoaded();
    m_skillTreesLoaded = true;
    std::vector<SkillTreeLayout> layouts;
    if (g_buabridge.GetSkillTreeLayouts(&layouts, &m_jobInheritance)) {
        for (SkillTreeLayout& layout : layouts) {
            m_skillTrees.emplace(layout.jobId, std::move(layout));
        }
    }
}

std::array<SkillTreeLayout, kSkillTreeJobTabs> CSkillMgr::GetSkillTreeTabs(int jobId)
{
    LoadSkillTrees();
    std::vector<const SkillTreeLayout*> lineage;
    std::vector<int> visited;
    // Only layouts in the character's ancestry belong in its job tabs. Aliases
    // without their own layout inherit the nearest configured class.
    while (std::find(visited.begin(), visited.end(), jobId) == visited.end()) {
        visited.push_back(jobId);
        const auto tree = m_skillTrees.find(jobId);
        if (tree != m_skillTrees.end() && (jobId != 0 || lineage.empty())) {
            lineage.push_back(&tree->second);
        }
        const auto parent = m_jobInheritance.find(jobId);
        if (parent == m_jobInheritance.end()) {
            break;
        }
        jobId = parent->second;
    }
    std::reverse(lineage.begin(), lineage.end());
    std::array<SkillTreeLayout, kSkillTreeJobTabs> tabs{};
    if (!lineage.empty()) {
        tabs[0] = *lineage.front();
    }
    if (lineage.size() > 1) {
        tabs[1] = *lineage[1];
        // Later jobs extend the second-job tree at their authored positions.
        // Keep its class titles and all cells not replaced by an extension.
        for (size_t index = 2; index < lineage.size(); ++index) {
            for (size_t cell = 0; cell < tabs[1].skillIds.size(); ++cell) {
                if (lineage[index]->skillIds[cell] > 0) {
                    tabs[1].skillIds[cell] = lineage[index]->skillIds[cell];
                }
            }
        }
    }
    for (SkillTreeLayout& tab : tabs) {
        for (size_t column = 0; column < tab.titles.size(); ++column) {
            if (tab.titles[column].empty()) {
                tab.titles[column] = "Tree " + std::to_string(column + 1);
            }
        }
    }
    return tabs;
}

bool CSkillMgr::LoadSkillMetadata()
{
    m_byId.clear();
    m_idBySkillName.clear();

    if (!BuildSkillIdMapFromNameTable()) {
        return false;
    }

    LoadSkillDisplayNames();
    LoadSkillDescriptions();
    LoadSkillLevelSpCosts();
    LoadLuaSkillMetadata();
    // Owned presentation names take precedence over legacy GRF labels in both
    // the skill UI and actor shouts.
    for (auto& entry : m_byId) {
        if (const SkillPresentation* presentation = FindSkillPresentation(entry.first)) {
            entry.second.displayName = presentation->name;
        }
    }
    return !m_byId.empty();
}

bool CSkillMgr::BuildSkillIdMapFromNameTable()
{
    // Wire-format skill IDs match the server (eAthena `e_skill` / packet SKID), not
    // skillnametable.txt line order. Ref resolves names by ID; we embed the same enum IDs.
    m_byId.clear();
    m_idBySkillName.clear();
    for (size_t i = 0; i < kSkillEnumEntriesCount; ++i) {
        const SkillEnumEntry& e = kSkillEnumEntries[i];
        SkillMetadata metadata;
        metadata.skillId = e.id;
        metadata.skillIdName = e.name;
        m_byId[e.id] = std::move(metadata);
        m_idBySkillName[ToLowerAscii(std::string(e.name))] = e.id;
    }
    return !m_byId.empty();
}

void CSkillMgr::LoadSkillDisplayNames()
{
    std::vector<std::string> lines;
    if (!ReadLines("skillnametable.txt", lines)) {
        return;
    }

    for (const std::string& rawLine : lines) {
        const std::string line = TrimAscii(rawLine);
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = SplitHashTokens(line);
        if (tokens.size() < 2) {
            continue;
        }

        const auto it = m_idBySkillName.find(ToLowerAscii(TrimAscii(tokens[0])));
        if (it == m_idBySkillName.end()) {
            continue;
        }

        SkillMetadata& metadata = m_byId[it->second];
        metadata.displayName = tokens[1];
        std::replace(metadata.displayName.begin(), metadata.displayName.end(), '_', ' ');
    }
}

void CSkillMgr::LoadSkillDescriptions()
{
    std::vector<std::string> lines;
    if (!ReadLines("skilldesctable.txt", lines)) {
        return;
    }

    SkillMetadata* currentMetadata = nullptr;
    bool expectTitle = false;
    for (const std::string& rawLine : lines) {
        const std::string trimmed = TrimAscii(rawLine);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed == "#") {
            currentMetadata = nullptr;
            expectTitle = false;
            continue;
        }

        if (trimmed.size() > 1 && trimmed.back() == '#') {
            const std::string skillIdName = trimmed.substr(0, trimmed.size() - 1);
            const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
            currentMetadata = (it == m_idBySkillName.end()) ? nullptr : &m_byId[it->second];
            expectTitle = true;
            continue;
        }

        if (!currentMetadata) {
            continue;
        }

        if (expectTitle) {
            if (currentMetadata->displayName.empty()) {
                currentMetadata->displayName = trimmed;
            }
            expectTitle = false;
            continue;
        }

        currentMetadata->descriptionLines.push_back(rawLine);
    }
}

void CSkillMgr::LoadSkillLevelSpCosts()
{
    std::vector<std::string> lines;
    if (!ReadLines("leveluseskillspamount.txt", lines)) {
        return;
    }

    SkillMetadata* currentMetadata = nullptr;
    for (const std::string& rawLine : lines) {
        const std::string trimmed = TrimAscii(rawLine);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed == "@") {
            currentMetadata = nullptr;
            continue;
        }

        if (trimmed.size() > 1 && trimmed.back() == '#') {
            const std::string skillIdName = trimmed.substr(0, trimmed.size() - 1);
            const auto it = m_idBySkillName.find(ToLowerAscii(skillIdName));
            currentMetadata = (it == m_idBySkillName.end()) ? nullptr : &m_byId[it->second];
            continue;
        }

        if (!currentMetadata) {
            continue;
        }

        currentMetadata->levelSpCosts.push_back(std::atoi(trimmed.c_str()));
    }
}

void CSkillMgr::LoadLuaSkillMetadata()
{
    const auto loadScript = [](const char* name) {
        const std::string base = std::string("lua files\\skillinfoz\\") + name;
        return g_buabridge.LoadRagnarokScriptOnce((base + ".lub").c_str())
            || g_buabridge.LoadRagnarokScriptOnce((base + ".lua").c_str());
    };
    loadScript("skillid");
    loadScript("jobinheritlist"); // Defines JOBID used by per-class prerequisites.
    const bool hasInfo = loadScript("skillinfolist");
    const bool hasDescriptions = loadScript("skilldescript");
    if (!hasInfo && !hasDescriptions) {
        return;
    }

    std::vector<SkillMetadata> entries;
    if (!g_buabridge.GetSkillMetadata(&entries)) {
        return;
    }
    for (const SkillMetadata& entry : entries) {
        SkillMetadata& metadata = m_byId[entry.skillId];
        metadata.skillId = entry.skillId;
        if (!entry.skillIdName.empty()) {
            metadata.skillIdName = entry.skillIdName;
            m_idBySkillName[ToLowerAscii(entry.skillIdName)] = entry.skillId;
        }
        if (!entry.displayName.empty()) {
            metadata.displayName = entry.displayName;
        }
        if (entry.maxLevel > 0) {
            metadata.maxLevel = entry.maxLevel;
        }
        metadata.separateLevels = entry.separateLevels;
        if (!entry.descriptionLines.empty()) {
            metadata.descriptionLines = entry.descriptionLines;
        }
        if (!entry.levelSpCosts.empty()) {
            metadata.levelSpCosts = entry.levelSpCosts;
        }
    }
}

CSkillMgr g_skillMgr;
