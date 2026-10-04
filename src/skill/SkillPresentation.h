#pragma once

#include <string>
#include <vector>

// Loaded from our loose skill_presentation.lua, independently of GRF skill scripts.
struct SkillEffectVariant {
    std::string strPath;
    std::string wavPath;
};

// A reusable procedural presentation for effects without an authored STR.
struct SkillBodyBurstPresentation {
    std::string texturePath;
    std::string wavPath;
    int flashStartColor = 0xFFFFFF;
    int flashPeakColor = 0xFFFFFF;
    int flashPeakMs = 0;
    int flashEndMs = 0;
    float haloAlpha = 0.0f;
    float haloRadius = 0.0f;
    int burstDelayMs = 0;
    int burstDurationMs = 0;
    int burstCount = 0;
    float burstArcDegrees = 0.0f;
    float burstHeight = 0.0f;
    float burstAlpha = 0.0f;
    std::vector<float> burstRadii;
};

struct SkillEffectPresentation {
    enum class Renderer { Str, BodyBurst };
    Renderer renderer = Renderer::Str;
    std::vector<SkillEffectVariant> variants;
    SkillBodyBurstPresentation bodyBurst;
    float offsetY = 0.0f;
    bool loop = false;
};

struct SkillPresentation {
    std::string name;
    bool passive = false;
    bool sourceServerNotified = false;
    bool targetServerNotified = false;
    bool groundServerNotified = false;
    int beginEffectId = -1;
    int motionType = 7;
    std::vector<int> sourceEffects;
    std::vector<int> targetEffects;
    std::vector<int> groundEffects;
};

// Returned definitions remain valid for the lifetime of the process.
const SkillPresentation* FindSkillPresentation(int skillId);
const SkillEffectPresentation* FindSkillEffectPresentation(int effectId);
