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
        if (!entry.descriptionLines.empty()) {
            metadata.descriptionLines = entry.descriptionLines;
        }
        if (!entry.levelSpCosts.empty()) {
            metadata.levelSpCosts = entry.levelSpCosts;
        }
    }
}

CSkillMgr g_skillMgr;
