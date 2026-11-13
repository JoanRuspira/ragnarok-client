#pragma once

#include <string>
#include <vector>

// Loaded from our loose skill_presentation.lua, independently of GRF skill scripts.
struct SkillEffectVariant {
    std::string strPath;
    std::string wavPath;
};

// Authored screen-space clips aimed between a caster and a skill target.
struct SkillDirectionalStrVariant {
    std::string strPath;
    float originX = 0.0f;
    float originY = 0.0f;
    float endX = 0.0f;
    float endY = 0.0f;
};

struct SkillDirectionalStrPresentation {
    std::vector<SkillDirectionalStrVariant> directions;
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

// A brief textured barrier for effects such as EF_MAXPAIN, which have no STR.
struct SkillGuardSpherePresentation {
    std::string texturePath;
    std::string wavPath;
    int durationMs = 700;
    int fadeInMs = 100;
    int fadeOutMs = 200;
    int color = 0xFF9F24;
    float radius = 7.0f;
    float height = 16.0f;
    float alpha = 0.8f;
};

// Traveling crescents for Sonic Wave, whose GRF contains textures but no STR.
struct SkillSonicWavePresentation {
    std::string texturePath;
    std::string trailTexturePath;
    std::string wavPath;
    int waveCount = 3;
    int waveIntervalMs = 336;
    int waveDurationMs = 360;
    float radius = 12.0f;
    float height = 14.0f;
};

// A textured ground marker, sized in map cells rather than screen pixels.
struct SkillGroundCirclePresentation {
    std::string texturePath;
    float radiusCells = 4.5f;
    float rotationSpeed = 1.0f; // Radians per second.
    int color = 0xFFFFFF; // RGB tint; white preserves the texture's authored colors.
};

struct SkillEffectPresentation {
    enum class Renderer { Str, DirectionalStr, BodyBurst, GuardSphere, SonicWave, GroundCircle };
    Renderer renderer = Renderer::Str;
    std::vector<SkillEffectVariant> variants;
    SkillDirectionalStrPresentation directionalStr;
    SkillBodyBurstPresentation bodyBurst;
    SkillGuardSpherePresentation guardSphere;
    SkillSonicWavePresentation sonicWave;
    SkillGroundCirclePresentation groundCircle;
    std::string loopWavPath; // Plays while this effect is looping, independently of visual cadence.
    bool bottomEffect = false;
    bool drawOverActors = false;
    float bottomOffsetPx = 4.0f;
    float offsetY = 0.0f;
    int hueShiftDegrees = 0; // STR texture and layer RGB; 0 preserves authored colors.
    int saturationPercent = 100; // STR color saturation; 0 makes a white/gray glow.
    bool groundPlane = false; // Project STR coordinates onto the world XZ plane.
    float rotationSpeed = 0.0f; // Additional STR rotation, in radians per second.
    bool loop = false;
    int playCount = 1; // Total STR playbacks, including the first.
    bool oncePerHit = false;
    int delayMs = 0;
};

struct SkillPresentation {
    std::string name;
    bool passive = false;
    // silence_hit_sound = false disables hit audio; omitted or true enables it.
    bool hitSoundEnabled = true;
    std::string hitWavPath; // Optional replacement for the default per-hit WAV.
    bool sourceServerNotified = false;
    bool sourceOncePerCast = false; // Later impact notifications preserve the caster's current action.
    bool targetServerNotified = false;
    bool groundServerNotified = false;
    int beginEffectId = -1;
    int motionType = 7;
    // Explicit stage motions bypass legacy motion substitutions. -1 uses motionType.
    // Motion 1 holds the skill invocation pose at either stage.
    int castMotionType = -1;
    int skillMotionType = -1;
    std::vector<int> castingEffects;
    std::vector<int> castingGroundEffects;
    std::vector<int> sourceEffects;
    std::vector<int> targetEffects;
    std::vector<int> groundEffects;
};

// Audio can be configured without taking ownership of a skill's existing visuals.
struct SkillSoundPresentation {
    enum class Trigger { NoDamage, Damage, Ground };
    std::string wavPath;
    Trigger trigger = Trigger::NoDamage;
};

// Returned definitions remain valid for the lifetime of the process.
const SkillPresentation* FindSkillPresentation(int skillId);
const SkillEffectPresentation* FindSkillEffectPresentation(int effectId);
const SkillSoundPresentation* FindSkillSoundPresentation(int skillId);
