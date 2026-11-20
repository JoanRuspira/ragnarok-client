#include "SkillPresentation.h"

#include "DebugLog.h"
#include "core/SettingsIni.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <utility>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

namespace {

int AbsoluteIndex(lua_State* state, int index)
{
    return index > 0 ? index : lua_gettop(state) + index + 1;
}

bool ReadString(lua_State* state, int table, const char* key, std::string& value, bool required,
    std::string& error)
{
    lua_getfield(state, table, key);
    bool valid = !required && lua_isnil(state, -1);
    if (lua_type(state, -1) == LUA_TSTRING) {
        size_t size = 0;
        const char* bytes = lua_tolstring(state, -1, &size);
        value.assign(bytes, size);
        valid = !required || !value.empty();
    }
    lua_pop(state, 1);
    if (!valid) error = std::string("expected string field '") + key + "'";
    return valid;
}

bool ReadInteger(lua_State* state, int table, const char* key, int& value, std::string& error)
{
    lua_getfield(state, table, key);
    bool valid = lua_isnil(state, -1);
    if (lua_type(state, -1) == LUA_TNUMBER) {
        const double number = lua_tonumber(state, -1);
        valid = std::isfinite(number) && number >= -1 && number <= (std::numeric_limits<int>::max)()
            && number == std::floor(number);
        if (valid) value = static_cast<int>(number);
    }
    lua_pop(state, 1);
    if (!valid) error = std::string("expected integer field '") + key + "'";
    return valid;
}

bool ReadBoolean(lua_State* state, int table, const char* key, bool& value, std::string& error)
{
    lua_getfield(state, table, key);
    const bool valid = lua_isnil(state, -1) || lua_isboolean(state, -1);
    if (lua_isboolean(state, -1)) value = lua_toboolean(state, -1) != 0;
    lua_pop(state, 1);
    if (!valid) error = std::string("expected boolean field '") + key + "'";
    return valid;
}

bool ReadSkillMotionType(lua_State* state, int table, const char* key, int& value, std::string& error)
{
    if (!ReadInteger(state, table, key, value, error)) return false;
    switch (value) {
    case -1: // Use the legacy motion_type behavior.
    case 0:  // Idle.
    case 1:  // Skill invocation; holds the initial pose at either stage.
    case 2:  // Attack.
    case 5:  // Pick up.
    case 7:  // Skill.
    case 8:  // Cast.
    case 9:  // Second attack.
    case 16: // Dance.
    case 17: // Sing.
        return true;
    default:
        error = std::string("unsupported motion type in field '") + key + "'";
        return false;
    }
}

bool ReadIdKey(lua_State* state, int& id, std::string& error, int minimum = 1)
{
    if (lua_type(state, -2) == LUA_TNUMBER) {
        const double number = lua_tonumber(state, -2);
        if (std::isfinite(number) && number >= minimum && number <= (std::numeric_limits<int>::max)()
            && number == std::floor(number)) {
            id = static_cast<int>(number);
            return true;
        }
    }
    error = "expected a numeric ID >= " + std::to_string(minimum) + " as table key";
    return false;
}

bool ValidateResourcePath(std::string& path, const char* prefix, const char* extension,
    std::string& error)
{
    std::replace(path.begin(), path.end(), '/', '\\');
    if (path.rfind(prefix, 0) != 0 || path.size() >= 260 || path.find('\0') != std::string::npos
        || path.find("\\..\\") != std::string::npos || path.size() < 4
        || path.compare(path.size() - 4, 4, extension) != 0) {
        error = "expected a full GRF resource path: " + path;
        return false;
    }
    return true;
}

bool ReadVariant(lua_State* state, int table, SkillEffectVariant& variant, std::string& error)
{
    table = AbsoluteIndex(state, table);
    return lua_istable(state, table)
        && ReadString(state, table, "str", variant.strPath, true, error)
        && ReadString(state, table, "wav", variant.wavPath, false, error)
        && ValidateResourcePath(variant.strPath, "data\\texture\\effect\\", ".str", error)
        && (variant.wavPath.empty() || ValidateResourcePath(variant.wavPath, "data\\wav\\", ".wav", error));
}

std::string NormalizeWavePath(std::string path)
{
    for (char& character : path) {
        if (character == '/') character = '\\';
        else if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
    }
    if (path.rfind("data\\wav\\", 0) == 0) path.erase(0, 9);
    else if (path.rfind("wav\\", 0) == 0) path.erase(0, 4);
    return path;
}

bool ReadNumber(lua_State* state, int table, const char* key, float& value,
    float minimum, float maximum, std::string& error)
{
    lua_getfield(state, table, key);
    const double number = lua_tonumber(state, -1);
    const bool valid = lua_type(state, -1) == LUA_TNUMBER && std::isfinite(number)
        && number >= minimum && number <= maximum;
    if (valid) value = static_cast<float>(number);
    lua_pop(state, 1);
    if (!valid) error = std::string("invalid numeric field '") + key + "'";
    return valid;
}

bool ReadBodyBurst(lua_State* state, int table, SkillBodyBurstPresentation& burst, std::string& error)
{
    if (!ReadString(state, table, "texture", burst.texturePath, true, error)
        || !ValidateResourcePath(burst.texturePath, "data\\texture\\effect\\", ".tga", error)
        || !ReadString(state, table, "wav", burst.wavPath, false, error)
        || (!burst.wavPath.empty() && !ValidateResourcePath(burst.wavPath, "data\\wav\\", ".wav", error))
        || !ReadInteger(state, table, "flash_start_color", burst.flashStartColor, error)
        || !ReadInteger(state, table, "flash_peak_color", burst.flashPeakColor, error)
        || !ReadInteger(state, table, "flash_peak_ms", burst.flashPeakMs, error)
        || !ReadInteger(state, table, "flash_end_ms", burst.flashEndMs, error)
        || !ReadInteger(state, table, "burst_delay_ms", burst.burstDelayMs, error)
        || !ReadInteger(state, table, "burst_duration_ms", burst.burstDurationMs, error)
        || !ReadInteger(state, table, "burst_count", burst.burstCount, error)
        || !ReadNumber(state, table, "halo_alpha", burst.haloAlpha, 0.0f, 1.0f, error)
        || !ReadNumber(state, table, "halo_radius", burst.haloRadius, 0.0f, 20.0f, error)
        || !ReadNumber(state, table, "burst_arc_degrees", burst.burstArcDegrees, 0.1f, 180.0f, error)
        || !ReadNumber(state, table, "burst_height", burst.burstHeight, -1000.0f, 1000.0f, error)
        || !ReadNumber(state, table, "burst_alpha", burst.burstAlpha, 0.0f, 1.0f, error)) return false;
    if (burst.flashStartColor < 0 || burst.flashStartColor > 0xFFFFFF
        || burst.flashPeakColor < 0 || burst.flashPeakColor > 0xFFFFFF
        || burst.flashPeakMs < 0 || burst.flashPeakMs >= burst.flashEndMs || burst.flashEndMs > 60000
        || burst.burstDelayMs < 0 || burst.burstDelayMs > 60000
        || burst.burstDurationMs <= 0 || burst.burstDurationMs > 60000
        || burst.burstCount <= 0 || burst.burstCount > 64) {
        error = "invalid body_burst color, timing, or count";
        return false;
    }
    lua_getfield(state, table, "burst_radii");
    if (!lua_istable(state, -1) || lua_objlen(state, -1) == 0 || lua_objlen(state, -1) > 8) {
        error = "expected 1 to 8 burst_radii";
        return false;
    }
    const int radii = lua_gettop(state);
    const size_t count = lua_objlen(state, radii);
    for (size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, radii, static_cast<int>(index));
        const double radius = lua_tonumber(state, -1);
        if (lua_type(state, -1) != LUA_TNUMBER || !std::isfinite(radius) || radius <= 0 || radius > 1000) {
            error = "expected positive finite burst radius <= 1000";
            return false;
        }
        burst.burstRadii.push_back(static_cast<float>(radius));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return true;
}

bool ReadGuardSphere(lua_State* state, int table, SkillGuardSpherePresentation& sphere, std::string& error)
{
    if (!ReadString(state, table, "texture", sphere.texturePath, true, error)
        || !ValidateResourcePath(sphere.texturePath, "data\\texture\\effect\\", ".tga", error)
        || !ReadString(state, table, "wav", sphere.wavPath, false, error)
        || (!sphere.wavPath.empty() && !ValidateResourcePath(sphere.wavPath, "data\\wav\\", ".wav", error))
        || !ReadInteger(state, table, "duration_ms", sphere.durationMs, error)
        || !ReadInteger(state, table, "fade_in_ms", sphere.fadeInMs, error)
        || !ReadInteger(state, table, "fade_out_ms", sphere.fadeOutMs, error)
        || !ReadInteger(state, table, "color", sphere.color, error)
        || !ReadNumber(state, table, "radius", sphere.radius, 0.1f, 1000.0f, error)
        || !ReadNumber(state, table, "height", sphere.height, 0.1f, 1000.0f, error)
        || !ReadNumber(state, table, "alpha", sphere.alpha, 0.0f, 1.0f, error)) return false;
    if (sphere.durationMs <= 0 || sphere.durationMs > 60000
        || sphere.fadeInMs <= 0 || sphere.fadeOutMs <= 0
        || sphere.fadeInMs > sphere.durationMs || sphere.fadeOutMs > sphere.durationMs - sphere.fadeInMs
        || sphere.color < 0 || sphere.color > 0xFFFFFF) {
        error = "invalid guard_sphere color or timing";
        return false;
    }
    return true;
}

bool ReadSonicWave(lua_State* state, int table, SkillSonicWavePresentation& wave, std::string& error)
{
    if (!ReadString(state, table, "texture", wave.texturePath, true, error)
        || !ValidateResourcePath(wave.texturePath, "data\\texture\\effect\\", ".tga", error)
        || !ReadString(state, table, "trail_texture", wave.trailTexturePath, true, error)
        || !ValidateResourcePath(wave.trailTexturePath, "data\\texture\\effect\\", ".tga", error)
        || !ReadString(state, table, "wav", wave.wavPath, false, error)
        || (!wave.wavPath.empty() && !ValidateResourcePath(wave.wavPath, "data\\wav\\", ".wav", error))
        || !ReadInteger(state, table, "wave_count", wave.waveCount, error)
        || !ReadInteger(state, table, "wave_interval_ms", wave.waveIntervalMs, error)
        || !ReadInteger(state, table, "wave_duration_ms", wave.waveDurationMs, error)
        || !ReadNumber(state, table, "radius", wave.radius, 0.1f, 1000.0f, error)
        || !ReadNumber(state, table, "height", wave.height, 0.1f, 1000.0f, error)) return false;
    if (wave.waveCount <= 0 || wave.waveCount > 8
        || wave.waveIntervalMs < 0 || wave.waveIntervalMs > 5000
        || wave.waveDurationMs <= 0 || wave.waveDurationMs > 5000) {
        error = "invalid sonic_wave timing or count";
        return false;
    }
    return true;
}

bool ReadDirectionalStr(lua_State* state, int table, SkillDirectionalStrPresentation& effect, std::string& error)
{
    if (!ReadString(state, table, "wav", effect.wavPath, false, error)
        || (!effect.wavPath.empty() && !ValidateResourcePath(effect.wavPath, "data\\wav\\", ".wav", error))) return false;
    lua_getfield(state, table, "directions");
    if (!lua_istable(state, -1) || lua_objlen(state, -1) == 0 || lua_objlen(state, -1) > 8) {
        error = "expected between one and eight directional STR clips";
        return false;
    }
    const int directions = lua_gettop(state);
    const size_t count = lua_objlen(state, directions);
    for (size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, directions, static_cast<int>(index));
        const int entry = lua_gettop(state);
        SkillDirectionalStrVariant variant;
        if (!lua_istable(state, entry)
            || !ReadString(state, entry, "str", variant.strPath, true, error)
            || !ValidateResourcePath(variant.strPath, "data\\texture\\effect\\", ".str", error)
            || !ReadNumber(state, entry, "origin_x", variant.originX, -10000.0f, 10000.0f, error)
            || !ReadNumber(state, entry, "origin_y", variant.originY, -10000.0f, 10000.0f, error)
            || !ReadNumber(state, entry, "end_x", variant.endX, -10000.0f, 10000.0f, error)
            || !ReadNumber(state, entry, "end_y", variant.endY, -10000.0f, 10000.0f, error)) return false;
        const float dx = variant.endX - variant.originX;
        const float dy = variant.endY - variant.originY;
        if (dx * dx + dy * dy < 1.0f) {
            error = "directional STR endpoints must be at least one pixel apart";
            return false;
        }
        effect.directions.push_back(std::move(variant));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return true;
}

bool ReadAdditionalStrs(lua_State* state, int table, std::vector<std::string>& paths, std::string& error)
{
    lua_getfield(state, table, "additional_strs");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return true;
    }
    if (!lua_istable(state, -1) || lua_objlen(state, -1) > 16) {
        error = "expected an additional_strs array with at most 16 STR paths";
        return false;
    }
    const int entries = lua_gettop(state);
    const size_t count = lua_objlen(state, entries);
    for (size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, entries, static_cast<int>(index));
        if (lua_type(state, -1) != LUA_TSTRING) {
            error = "expected a string path in additional_strs";
            return false;
        }
        size_t size = 0;
        const char* bytes = lua_tolstring(state, -1, &size);
        std::string path(bytes, size);
        lua_pop(state, 1);
        if (!ValidateResourcePath(path, "data\\texture\\effect\\", ".str", error)) return false;
        paths.push_back(std::move(path));
    }
    lua_pop(state, 1);
    return true;
}

bool ReadSkillLevelTints(lua_State* state, int table, std::map<int, int>& tints, std::string& error)
{
    lua_getfield(state, table, "skill_level_tints");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        return true;
    }
    if (!lua_istable(state, -1)) {
        error = "expected a skill_level_tints table";
        return false;
    }
    const int entries = lua_gettop(state);
    lua_pushnil(state);
    while (lua_next(state, entries)) {
        int level = 0;
        const double tint = lua_tonumber(state, -1);
        if (!ReadIdKey(state, level, error) || level > 65535 || lua_type(state, -1) != LUA_TNUMBER
            || !std::isfinite(tint) || tint < 0 || tint > 0xFFFFFF || tint != std::floor(tint)) {
            error = "expected skill levels 1..65535 and RGB colors in skill_level_tints";
            return false;
        }
        tints.emplace(level, static_cast<int>(tint));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return true;
}

bool ReadEffect(lua_State* state, int table, SkillEffectPresentation& effect, std::string& error)
{
    table = AbsoluteIndex(state, table);
    if (!lua_istable(state, table)
        || !ReadBoolean(state, table, "loop", effect.loop, error)
        || !ReadInteger(state, table, "play_count", effect.playCount, error)
        || !ReadInteger(state, table, "fade_out_frame", effect.fadeOutFrame, error)
        || !ReadInteger(state, table, "fade_out_frames", effect.fadeOutFrames, error)
        || !ReadBoolean(state, table, "replay_wav", effect.replayWav, error)
        || !ReadBoolean(state, table, "ignore_server_wav", effect.ignoreServerWav, error)
        || !ReadString(state, table, "loop_wav", effect.loopWavPath, false, error)
        || (!effect.loopWavPath.empty()
            && !ValidateResourcePath(effect.loopWavPath, "data\\wav\\", ".wav", error))
        || !ReadBoolean(state, table, "once_per_hit", effect.oncePerHit, error)
        || !ReadInteger(state, table, "delay_ms", effect.delayMs, error)
        || !ReadInteger(state, table, "hue_shift_degrees", effect.hueShiftDegrees, error)
        || !ReadInteger(state, table, "saturation_percent", effect.saturationPercent, error)
        || !ReadInteger(state, table, "tint", effect.tint, error)
        || !ReadSkillLevelTints(state, table, effect.skillLevelTints, error)
        || !ReadBoolean(state, table, "ground_plane", effect.groundPlane, error)
        || !ReadBoolean(state, table, "bottom_effect", effect.bottomEffect, error)
        || !ReadInteger(state, table, "bottom_order", effect.bottomOrder, error)
        || !ReadBoolean(state, table, "draw_over_actors", effect.drawOverActors, error)) return false;
    if (effect.bottomOrder != 0 && (!effect.bottomEffect || effect.drawOverActors)) {
        error = "bottom_order requires a bottom effect beneath actors";
        return false;
    }
    if (effect.drawOverActors && !effect.bottomEffect) {
        error = "draw_over_actors requires bottom_effect = true";
        return false;
    }
    if (effect.playCount < 1 || (effect.loop && effect.playCount > 1)) {
        error = "play_count must be positive and cannot exceed 1 with loop = true";
        return false;
    }
    if (effect.fadeOutFrames < 0 || ((effect.fadeOutFrame >= 0) != (effect.fadeOutFrames > 0))
        || static_cast<long long>(effect.fadeOutFrame) + effect.fadeOutFrames > (std::numeric_limits<int>::max)() - 8LL) {
        error = "fade_out_frame requires a nonnegative frame and positive fade_out_frames within the frame range";
        return false;
    }
    if (effect.replayWav && effect.playCount <= 1) {
        error = "replay_wav requires play_count > 1";
        return false;
    }
    if (effect.delayMs < 0) {
        error = "expected a nonnegative delay_ms";
        return false;
    }
    if (effect.hueShiftDegrees < 0 || effect.hueShiftDegrees > 360) {
        error = "expected hue_shift_degrees between 0 and 360";
        return false;
    }
    if (effect.saturationPercent < 0 || effect.saturationPercent > 100) {
        error = "expected saturation_percent between 0 and 100";
        return false;
    }
    if (effect.tint < 0 || effect.tint > 0xFFFFFF) {
        error = "expected tint between 0x000000 and 0xFFFFFF";
        return false;
    }
    lua_getfield(state, table, "bottom_offset_px");
    if (!lua_isnil(state, -1)) {
        const double offset = lua_tonumber(state, -1);
        if (lua_type(state, -1) != LUA_TNUMBER || !std::isfinite(offset) || offset < 0 || offset > 10000) {
            error = "expected a finite bottom_offset_px between 0 and 10000";
            return false;
        }
        effect.bottomOffsetPx = static_cast<float>(offset);
    }
    lua_pop(state, 1);
    std::string renderer;
    if (!ReadString(state, table, "renderer", renderer, false, error)) return false;
    lua_getfield(state, table, "screen_offset_y_px");
    const bool hasScreenOffsetY = !lua_isnil(state, -1);
    lua_pop(state, 1);
    if (hasScreenOffsetY && !ReadNumber(state, table, "screen_offset_y_px",
            effect.screenOffsetYPx, -10000.0f, 10000.0f, error)) return false;
    if (effect.screenOffsetYPx != 0 && !renderer.empty() && renderer != "str" && renderer != "directional_str") {
        error = "screen_offset_y_px requires an STR renderer";
        return false;
    }
    if (!ReadAdditionalStrs(state, table, effect.additionalStrPaths, error)) return false;
    if (!effect.additionalStrPaths.empty() && !renderer.empty() && renderer != "str") {
        error = "additional_strs requires an STR renderer";
        return false;
    }
    if (effect.groundPlane && !renderer.empty() && renderer != "str") {
        error = "ground_plane requires an STR renderer";
        return false;
    }
    if (effect.playCount > 1 && !renderer.empty() && renderer != "str" && renderer != "directional_str") {
        error = "play_count requires an STR renderer";
        return false;
    }
    if (effect.fadeOutFrame >= 0 && !renderer.empty() && renderer != "str" && renderer != "directional_str") {
        error = "fade_out_frame requires an STR renderer";
        return false;
    }
    if (effect.drawOverActors && !renderer.empty() && renderer != "str" && renderer != "directional_str") {
        error = "draw_over_actors requires an STR renderer";
        return false;
    }
    if ((effect.hueShiftDegrees != 0 || effect.saturationPercent != 100 || effect.tint != 0xFFFFFF
            || !effect.skillLevelTints.empty())
            && !renderer.empty() && renderer != "str" && renderer != "directional_str") {
        error = "hue_shift_degrees, saturation_percent, and tint require an STR renderer";
        return false;
    }
    if (renderer == "body_burst") {
        effect.renderer = SkillEffectPresentation::Renderer::BodyBurst;
        if (effect.loop) {
            error = "body_burst cannot loop";
            return false;
        }
        return ReadBodyBurst(state, table, effect.bodyBurst, error);
    }
    if (!renderer.empty() && renderer != "str" && renderer != "directional_str"
        && renderer != "guard_sphere" && renderer != "sonic_wave" && renderer != "soul_exchange"
        && renderer != "flame_breath" && renderer != "ground_circle") {
        error = "unknown effect renderer: " + renderer;
        return false;
    }
    lua_getfield(state, table, "offset_y");
    if (!lua_isnil(state, -1)) {
        const double offset = lua_tonumber(state, -1);
        if (lua_type(state, -1) != LUA_TNUMBER || !std::isfinite(offset) || std::abs(offset) > 10000) {
            error = "expected a finite numeric offset_y";
            return false;
        }
        effect.offsetY = static_cast<float>(offset);
    }
    lua_pop(state, 1);
    if (renderer == "ground_circle") {
        effect.renderer = SkillEffectPresentation::Renderer::GroundCircle;
        if (!ReadInteger(state, table, "color", effect.groundCircle.color, error)) return false;
        if (effect.groundCircle.color < 0 || effect.groundCircle.color > 0xFFFFFF) {
            error = "ground_circle color must be between 0x000000 and 0xFFFFFF";
            return false;
        }
        lua_getfield(state, table, "rotation_speed");
        const bool hasRotationSpeed = !lua_isnil(state, -1);
        lua_pop(state, 1);
        return ReadString(state, table, "texture", effect.groundCircle.texturePath, true, error)
            && ValidateResourcePath(effect.groundCircle.texturePath, "data\\texture\\effect\\", ".tga", error)
            && ReadNumber(state, table, "radius_cells", effect.groundCircle.radiusCells, 0.1f, 100.0f, error)
            && (!hasRotationSpeed || ReadNumber(state, table, "rotation_speed",
                effect.groundCircle.rotationSpeed, -100.0f, 100.0f, error));
    }
    if (renderer == "directional_str") {
        effect.renderer = SkillEffectPresentation::Renderer::DirectionalStr;
        return ReadDirectionalStr(state, table, effect.directionalStr, error);
    }
    if (renderer == "flame_breath") {
        effect.renderer = SkillEffectPresentation::Renderer::FlameBreath;
        auto& breath = effect.flameBreath;
        if (effect.loop) {
            error = "flame_breath cannot loop";
            return false;
        }
        if (!ReadString(state, table, "wav", breath.wavPath, false, error)
            || (!breath.wavPath.empty() && !ValidateResourcePath(breath.wavPath, "data\\wav\\", ".wav", error))
            || !ReadInteger(state, table, "duration_ms", breath.durationMs, error)
            || !ReadInteger(state, table, "travel_ms", breath.travelMs, error)
            || !ReadInteger(state, table, "color", breath.color, error)
            || !ReadInteger(state, table, "core_color", breath.coreColor, error)
            || !ReadNumber(state, table, "radius", breath.radius, 0.1f, 32.0f, error)
            || !ReadNumber(state, table, "height", breath.height, 0.0f, 100.0f, error)) return false;
        if (breath.travelMs <= 0 || breath.travelMs >= breath.durationMs || breath.durationMs > 5000
            || breath.color < 0 || breath.color > 0xFFFFFF
            || breath.coreColor < 0 || breath.coreColor > 0xFFFFFF) {
            error = "invalid flame_breath timing or color";
            return false;
        }
        return true;
    }
    if (renderer == "soul_exchange") {
        effect.renderer = SkillEffectPresentation::Renderer::SoulExchange;
        auto& exchange = effect.soulExchange;
        if (effect.loop) {
            error = "soul_exchange cannot loop";
            return false;
        }
        if (!ReadInteger(state, table, "duration_ms", exchange.durationMs, error)
            || !ReadInteger(state, table, "color", exchange.color, error)
            || !ReadInteger(state, table, "core_color", exchange.coreColor, error)
            || !ReadBoolean(state, table, "target_to_source_only", exchange.targetToSourceOnly, error)
            || !ReadNumber(state, table, "radius", exchange.radius, 0.1f, 32.0f, error)
            || !ReadNumber(state, table, "height", exchange.height, 0.0f, 100.0f, error)) return false;
        if (exchange.durationMs <= 0 || exchange.durationMs > 5000
            || exchange.color < 0 || exchange.color > 0xFFFFFF
            || exchange.coreColor < 0 || exchange.coreColor > 0xFFFFFF) {
            error = "invalid soul_exchange duration or color";
            return false;
        }
        return true;
    }
    if (renderer == "sonic_wave") {
        effect.renderer = SkillEffectPresentation::Renderer::SonicWave;
        if (effect.loop) {
            error = "sonic_wave cannot loop";
            return false;
        }
        return ReadSonicWave(state, table, effect.sonicWave, error);
    }
    if (renderer == "guard_sphere") {
        effect.renderer = SkillEffectPresentation::Renderer::GuardSphere;
        if (effect.loop) {
            error = "guard_sphere cannot loop";
            return false;
        }
        return ReadGuardSphere(state, table, effect.guardSphere, error);
    }
    lua_getfield(state, table, "rotation_speed");
    const bool hasRotationSpeed = !lua_isnil(state, -1);
    lua_pop(state, 1);
    if (hasRotationSpeed && !ReadNumber(state, table, "rotation_speed", effect.rotationSpeed, -100.0f, 100.0f, error)) {
        return false;
    }
    lua_getfield(state, table, "variants");
    if (lua_isnil(state, -1)) {
        lua_pop(state, 1);
        SkillEffectVariant variant;
        if (!ReadVariant(state, table, variant, error)) return false;
        effect.variants.push_back(std::move(variant));
        return true;
    }
    if (!lua_istable(state, -1) || lua_objlen(state, -1) == 0) {
        error = "expected a nonempty variants array";
        return false;
    }
    const int variants = lua_gettop(state);
    const size_t count = lua_objlen(state, variants);
    for (size_t index = 1; index <= count; ++index) {
        lua_rawgeti(state, variants, static_cast<int>(index));
        SkillEffectVariant variant;
        if (!ReadVariant(state, -1, variant, error)) return false;
        effect.variants.push_back(std::move(variant));
        lua_pop(state, 1);
    }
    lua_pop(state, 1);
    return true;
}

bool ReadSkillSound(lua_State* state, int table, SkillSoundPresentation& sound, std::string& error)
{
    table = AbsoluteIndex(state, table);
    std::string trigger;
    if (!ReadString(state, table, "wav", sound.wavPath, true, error)
        || !ValidateResourcePath(sound.wavPath, "data\\wav\\", ".wav", error)
        || !ReadString(state, table, "trigger", trigger, false, error)
        || !ReadInteger(state, table, "play_count", sound.playCount, error)
        || !ReadInteger(state, table, "interval_ms", sound.intervalMs, error)
        || !ReadInteger(state, table, "delay_ms", sound.delayMs, error)
        || !ReadBoolean(state, table, "ignore_server_wav", sound.ignoreServerWav, error)) return false;
    const long long lastStartMs = static_cast<long long>(sound.delayMs)
        + (sound.playCount - 1LL) * sound.intervalMs;
    if (sound.playCount < 1 || sound.playCount > 1024 || sound.intervalMs < 0 || sound.delayMs < 0
        || (sound.playCount > 1 && sound.intervalMs == 0)) {
        error = "sound requires play_count 1..1024, nonnegative timing, and a positive interval_ms for repeats";
        return false;
    }
    if (lastStartMs > (std::numeric_limits<int>::max)()) {
        error = "sound's final playback delay must not exceed INT_MAX milliseconds";
        return false;
    }
    if (trigger == "damage") sound.trigger = SkillSoundPresentation::Trigger::Damage;
    else if (trigger == "ground") sound.trigger = SkillSoundPresentation::Trigger::Ground;
    else if (trigger == "ground_cast") sound.trigger = SkillSoundPresentation::Trigger::GroundCast;
    else if (trigger == "casting") sound.trigger = SkillSoundPresentation::Trigger::Casting;
    else if (!trigger.empty() && trigger != "no_damage") {
        error = "unknown skill sound trigger: " + trigger;
        return false;
    }
    return true;
}

struct PresentationCatalog {
    std::map<int, SkillPresentation> skills;
    std::map<int, SkillEffectPresentation> effects;
    std::map<int, SkillSoundPresentation> sounds;

    bool ReadSkillEffects(lua_State* state, int table, const char* key, std::vector<int>& ids,
        std::string& error)
    {
        lua_getfield(state, table, key);
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            return true;
        }
        if (!lua_istable(state, -1)) {
            error = std::string("expected effect array '") + key + "'";
            return false;
        }
        const int entries = lua_gettop(state);
        const size_t count = lua_objlen(state, entries);
        for (size_t index = 1; index <= count; ++index) {
            lua_rawgeti(state, entries, static_cast<int>(index));
            const int entry = lua_gettop(state);
            int effectId = -1;
            if (!lua_istable(state, entry) || !ReadInteger(state, entry, "effect_id", effectId, error)
                || effectId < 0) {
                error = "expected a nonnegative effect_id in each skill effect";
                return false;
            }
            lua_getfield(state, entry, "str");
            const bool hasStr = !lua_isnil(state, -1);
            lua_pop(state, 1);
            lua_getfield(state, entry, "variants");
            const bool hasVariants = !lua_isnil(state, -1);
            lua_pop(state, 1);
            lua_getfield(state, entry, "renderer");
            const bool hasRenderer = !lua_isnil(state, -1);
            lua_pop(state, 1);
            if (hasStr || hasVariants || hasRenderer) {
                SkillEffectPresentation effect;
                if (!ReadEffect(state, entry, effect, error)) return false;
                if (!effects.emplace(effectId, std::move(effect)).second) {
                    error = "duplicate effect definition: " + std::to_string(effectId);
                    return false;
                }
            }
            ids.push_back(effectId);
            lua_pop(state, 1);
        }
        lua_pop(state, 1);
        return true;
    }

    bool ReadSelectionEffects(lua_State* state, int table, std::map<int, int>& selections,
        std::string& error)
    {
        lua_getfield(state, table, "selection");
        if (lua_isnil(state, -1)) {
            lua_pop(state, 1);
            return true;
        }
        if (!lua_istable(state, -1)) {
            error = "expected a selection effects table";
            return false;
        }
        const int entries = lua_gettop(state);
        lua_pushnil(state);
        while (lua_next(state, entries)) {
            int skillId = 0;
            int effectId = -1;
            SkillEffectPresentation effect;
            if (!ReadIdKey(state, skillId, error) || skillId > 65535
                || !lua_istable(state, -1)
                || !ReadInteger(state, -1, "effect_id", effectId, error) || effectId < 0) {
                error = "expected a skill ID and nonnegative effect_id for selection";
                return false;
            }
            if (!ReadEffect(state, -1, effect, error)) return false;
            if (!effects.emplace(effectId, std::move(effect)).second) {
                error = "duplicate effect definition: " + std::to_string(effectId);
                return false;
            }
            selections.emplace(skillId, effectId);
            lua_pop(state, 1);
        }
        lua_pop(state, 1);
        return true;
    }

    bool Read(lua_State* state, std::string& error)
    {
        if (!lua_istable(state, 1)) {
            error = "file must return a table";
            return false;
        }
        int version = -1;
        if (!ReadInteger(state, 1, "version", version, error) || version != 1) {
            error = "expected version = 1";
            return false;
        }
        lua_getfield(state, 1, "sounds");
        if (!lua_isnil(state, -1)) {
            if (!lua_istable(state, -1)) {
                error = "expected sounds table";
                return false;
            }
            const int entries = lua_gettop(state);
            lua_pushnil(state);
            while (lua_next(state, entries)) {
                int id = 0;
                SkillSoundPresentation sound;
                const int entry = lua_gettop(state);
                if (!ReadIdKey(state, id, error) || id > 65535 || !lua_istable(state, entry)
                    || !ReadSkillSound(state, entry, sound, error)) return false;
                sounds.emplace(id, std::move(sound));
                lua_pop(state, 1);
            }
        }
        lua_pop(state, 1);
        lua_getfield(state, 1, "effects");
        if (!lua_isnil(state, -1)) {
            if (!lua_istable(state, -1)) {
                error = "expected effects table";
                return false;
            }
            const int entries = lua_gettop(state);
            lua_pushnil(state);
            while (lua_next(state, entries)) {
                int id = 0;
                SkillEffectPresentation effect;
                if (!ReadIdKey(state, id, error, 0) || !ReadEffect(state, -1, effect, error)) return false;
                effects.emplace(id, std::move(effect));
                lua_pop(state, 1);
            }
        }
        lua_pop(state, 1);
        lua_getfield(state, 1, "skills");
        if (!lua_istable(state, -1)) {
            error = "expected skills table";
            return false;
        }
        const int entries = lua_gettop(state);
        lua_pushnil(state);
        while (lua_next(state, entries)) {
            int id = 0;
            SkillPresentation skill;
            bool serverNotified = false;
            const int entry = lua_gettop(state);
            if (!ReadIdKey(state, id, error) || id > 65535 || !lua_istable(state, entry)
                || !ReadString(state, entry, "name", skill.name, true, error)
                || !ReadBoolean(state, entry, "passive", skill.passive, error)
                || !ReadBoolean(state, entry, "silence_hit_sound", skill.hitSoundEnabled, error)
                || !ReadInteger(state, entry, "damage_number_delay_ms", skill.damageNumberDelayMs, error)
                || !ReadString(state, entry, "hit_wav", skill.hitWavPath, false, error)
                || (!skill.hitWavPath.empty()
                    && !ValidateResourcePath(skill.hitWavPath, "data\\wav\\", ".wav", error))
                || !ReadBoolean(state, entry, "server_notified", serverNotified, error)
                || !ReadBoolean(state, entry, "source_presentation", skill.sourcePresentation, error)
                || !ReadBoolean(state, entry, "source_once_per_cast", skill.sourceOncePerCast, error)
                || !ReadInteger(state, entry, "begin_effect_id", skill.beginEffectId, error)
                || !ReadInteger(state, entry, "motion_type", skill.motionType, error)
                || !ReadSkillMotionType(state, entry, "cast_motion_type", skill.castMotionType, error)
                || !ReadSkillMotionType(state, entry, "skill_motion_type", skill.skillMotionType, error)
                || !ReadSkillEffects(state, entry, "casting", skill.castingEffects, error)
                || !ReadSkillEffects(state, entry, "casting_ground", skill.castingGroundEffects, error)
                || !ReadSkillEffects(state, entry, "casting_target", skill.castingTargetEffects, error)
                || !ReadSkillEffects(state, entry, "source", skill.sourceEffects, error)
                || !ReadSkillEffects(state, entry, "target", skill.targetEffects, error)
                || !ReadSkillEffects(state, entry, "ground", skill.groundEffects, error)
                || !ReadSelectionEffects(state, entry, skill.selectionEffects, error)) return false;
            if (skill.damageNumberDelayMs < 0) {
                error = "expected a nonnegative damage_number_delay_ms";
                return false;
            }
            skill.sourceServerNotified = serverNotified;
            skill.targetServerNotified = serverNotified;
            skill.groundServerNotified = serverNotified;
            if (!ReadBoolean(state, entry, "source_server_notified", skill.sourceServerNotified, error)
                || !ReadBoolean(state, entry, "target_server_notified", skill.targetServerNotified, error)
                || !ReadBoolean(state, entry, "ground_server_notified", skill.groundServerNotified, error)) return false;
            skills.emplace(id, std::move(skill));
            lua_pop(state, 1);
        }
        // Validate references after every inline definition is registered; Lua table order is unspecified.
        for (const auto& entry : skills) {
            const SkillPresentation& skill = entry.second;
            if (skill.passive && (sounds.count(entry.first) || !skill.hitWavPath.empty())) {
                error = "passive skill must have no presentation sound";
                return false;
            }
            if (skill.passive && skill.beginEffectId >= 0) {
                error = "passive skill must have no cast-start effect";
                return false;
            }
            if (skill.passive && !skill.selectionEffects.empty()) {
                error = "passive skill must have no selection effects";
                return false;
            }
            for (const auto* ids : { &skill.castingEffects, &skill.castingGroundEffects,
                    &skill.castingTargetEffects, &skill.sourceEffects, &skill.targetEffects, &skill.groundEffects }) {
                for (int effectId : *ids) {
                    if (!effects.count(effectId)) {
                        error = "undefined effect reference: " + std::to_string(effectId);
                        return false;
                    }
                }
                if (skill.passive && !ids->empty()) {
                    error = "passive skill must have no presentation effects";
                    return false;
                }
            }
        }
        return true;
    }

    PresentationCatalog()
    {
        std::vector<std::filesystem::path> candidates = {
            GetOpenMidgardIniPath().parent_path() / "skill_presentation.lua",
        };
        std::error_code ec;
        const auto current = std::filesystem::current_path(ec);
        if (!ec) candidates.push_back(current / "skill_presentation.lua");
#ifdef RO_SOURCE_ROOT
        candidates.emplace_back(std::filesystem::path(RO_SOURCE_ROOT) / "skill_presentation.lua");
#endif
        std::filesystem::path source;
        for (const auto& candidate : candidates) {
            if (std::filesystem::is_regular_file(candidate, ec) && !ec) {
                source = candidate;
                break;
            }
        }
        if (source.empty()) {
            DbgLog("[SkillPresentation] skill_presentation.lua not found.\n");
            return;
        }
        // Our own state: this table needs neither standard libraries nor legacy GRF skill scripts.
        std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), lua_close);
        std::string error = "could not create Lua state";
        bool loaded = false;
        if (state) {
            const std::string path = source.string();
            if (luaL_loadfile(state.get(), path.c_str()) != 0 || lua_pcall(state.get(), 0, 1, 0) != 0) {
                const char* message = lua_tostring(state.get(), -1);
                error = message ? message : "could not read presentation table";
            } else {
                error = "invalid presentation definition";
                loaded = Read(state.get(), error);
            }
        }
        if (!loaded) {
            skills.clear();
            effects.clear();
            sounds.clear();
            DbgLog("[SkillPresentation] rejected '%s': %s\n", source.string().c_str(), error.c_str());
            return;
        }
        DbgLog("[SkillPresentation] loaded '%s': %zu skills, %zu effects, %zu sounds\n",
            source.string().c_str(), skills.size(), effects.size(), sounds.size());
    }
};

const PresentationCatalog& Catalog()
{
    static const PresentationCatalog catalog;
    return catalog;
}

} // namespace

const SkillPresentation* FindSkillPresentation(int skillId)
{
    const auto& skills = Catalog().skills;
    const auto found = skills.find(skillId);
    return found == skills.end() ? nullptr : &found->second;
}

const SkillEffectPresentation* FindSkillEffectPresentation(int effectId)
{
    const auto& effects = Catalog().effects;
    const auto found = effects.find(effectId);
    return found == effects.end() ? nullptr : &found->second;
}

const SkillSoundPresentation* FindSkillSoundPresentation(int skillId)
{
    const auto& sounds = Catalog().sounds;
    const auto found = sounds.find(skillId);
    return found == sounds.end() ? nullptr : &found->second;
}

bool IsServerSkillWaveIgnored(const std::string& wavPath)
{
    const std::string normalizedPath = NormalizeWavePath(wavPath);
    if (normalizedPath.empty()) return false;
    const auto matches = [&normalizedPath](const std::string& configuredPath) {
        return !configuredPath.empty() && NormalizeWavePath(configuredPath) == normalizedPath;
    };
    for (const auto& entry : Catalog().sounds) {
        if (entry.second.ignoreServerWav && matches(entry.second.wavPath)) return true;
    }
    for (const auto& entry : Catalog().effects) {
        const SkillEffectPresentation& effect = entry.second;
        if (!effect.ignoreServerWav) continue;
        if (matches(effect.directionalStr.wavPath) || matches(effect.bodyBurst.wavPath)
            || matches(effect.guardSphere.wavPath) || matches(effect.sonicWave.wavPath)
            || matches(effect.flameBreath.wavPath)) return true;
        for (const SkillEffectVariant& variant : effect.variants) {
            if (matches(variant.wavPath)) return true;
        }
    }
    return false;
}
