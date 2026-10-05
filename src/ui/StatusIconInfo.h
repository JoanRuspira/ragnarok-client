#pragma once

#include "core/File.h"
#include "lua/LuaBridge.h"
#include "qtui/QtUiStatusIconCatalog.h"
#include "skill/Skill.h"

#include <string>
#include <vector>

namespace statusui {

inline std::string ResolveStatusIconLabel(int statusType)
{
    LuaStatusIconInfo info;
    if (g_buabridge.GetStatusIconInfoByStatusType(statusType, &info) && !info.displayName.empty()) {
        return info.displayName;
    }
    if (const auto* entry = qtui::FindStatusIconCatalogEntry(statusType)) {
        if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(entry->skillId)) {
            if (!metadata->displayName.empty()) return metadata->displayName;
        }
        return entry->fallbackLabel;
    }
    return "Status " + std::to_string(statusType);
}

inline std::string ResolveStatusIconPath(int statusType)
{
    LuaStatusIconInfo info;
    g_buabridge.GetStatusIconInfoByStatusType(statusType, &info);
    // Explicit GRF paths are authoritative, including placeholders for new art.
    if (!info.iconGrfPath.empty()) return info.iconGrfPath;
    const auto* entry = qtui::FindStatusIconCatalogEntry(statusType);
    std::vector<std::string> fileNames;
    if (!info.iconFileName.empty()) fileNames.push_back(info.iconFileName);
    if (entry && entry->legacyIconFileName && *entry->legacyIconFileName) {
        fileNames.emplace_back(entry->legacyIconFileName);
    }
    const char* prefixes[] = {
        "", "effect\\", "texture\\effect\\", "data\\texture\\effect\\", "data\\effect\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\effect\\"
    };
    for (const std::string& fileName : fileNames) {
        for (const char* prefix : prefixes) {
            const std::string path = std::string(prefix) + fileName;
            if (g_fileMgr.IsDataExist(path.c_str())) return path;
        }
    }
    if (entry && entry->skillId > 0) {
        const std::string path = g_skillMgr.GetSkillIconPath(entry->skillId);
        if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) return path;
    }
    return {};
}

} // namespace statusui
