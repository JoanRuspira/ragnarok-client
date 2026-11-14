#include "GameActor.h"
#include "MsgEffect.h"
#include "RagEffect.h"
#include "World.h"
#include "audio/Audio.h"
#include "core/ClientInfoLocale.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "DebugLog.h"
#include "render/DC.h"
#include "render/SpriteInterpolation.h"
#include "render3d/GraphicsSettings.h"
#include "render3d/Device.h"
#include "render3d/RenderDevice.h"
#include "res/ImfRes.h"
#include "res/PaletteRes.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "session/Session.h"
#include "ui/UIPcGage.h"
#include "ui/UIRechargeGage.h"
#include "ui/UIWindowMgr.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {

constexpr int kSkillIdRend = 953;
// Screen-pixel gap between the actor's top and the casting bar's bottom.
constexpr int kCastingBarActorGapPx = 4;

void SetTextureDebugName(CTexture* texture, const char* name)
{
    if (!texture || !name || name[0] == '\0') {
        return;
    }

    std::strncpy(texture->m_texName, name, sizeof(texture->m_texName) - 1);
    texture->m_texName[sizeof(texture->m_texName) - 1] = '\0';
}

void SetActorBillboardDebugName(CTexture* texture,
    const CPc& actor,
    int displayJob,
    int bodyAction,
    int headMotion,
    bool playerStyle)
{
    if (!texture) {
        return;
    }

    std::snprintf(texture->m_texName,
        sizeof(texture->m_texName),
        "__actor_billboard__:gid=%u:job=%d:act=%d:mot=%d:pc=%d",
        static_cast<unsigned int>(actor.m_gid),
        displayJob,
        bodyAction,
        headMotion,
        playerStyle ? 1 : 0);
    texture->m_texName[sizeof(texture->m_texName) - 1] = '\0';
}

float g_runtimeActorCameraLongitude = 0.0f;

constexpr int kPlayerBillboardComposeWidth = 128;
constexpr int kPlayerBillboardComposeHeight = 128;
constexpr int kPlayerBillboardAnchorX = 64;
constexpr int kPlayerBillboardAnchorY = 110;
constexpr int kSharedPlayerWarmupMotionLimit = 32;
constexpr int kItemBillboardComposeWidth = 96;
constexpr int kItemBillboardComposeHeight = 96;
constexpr int kItemBillboardAnchorX = 48;
constexpr int kItemBillboardAnchorY = 72;
constexpr float kGroundItemScreenScale = 1.6f;
constexpr bool kLogActLoad = false;
constexpr bool kLogMotionProgress = false;
constexpr bool kLogBillboardDiagnostics = false;
constexpr size_t kSharedBillboardCacheBudget = 64u * 1024u * 1024u;
constexpr int kJobWarpNpc = 0x2D;
constexpr int kJobWarpPortal = 0x80;
constexpr int kJobPreWarpPortal = 0x81;
constexpr u32 kRemoteMoveStallLogThresholdMs = 250;
constexpr u32 kRuwachRelaunchTicks = 200;
constexpr int kEffectStateRuwachMask = 0x2000;
constexpr int kEffectStateHidingMask = 0x0004;
constexpr int kEffectStateSpecialHidingMask = 0x0040;
constexpr int kEffectStateBurrowMask = 0x0002;
constexpr u32 kHideFadeDurationMs = 180;

struct MoveStallTraceState {
    float lastPosX = 0.0f;
    float lastPosZ = 0.0f;
    u32 lastMoveWallTick = 0;
    u32 lastLogWallTick = 0;
    bool wasStalled = false;
};

std::map<u32, MoveStallTraceState> g_moveStallTraceByGid;
constexpr int kJobHiddenWarpNpc = 0x8B;
constexpr const char* kLegacyMonsterSpriteRoot = "data\\sprite\\\xB8\xF3\xBD\xBA\xC5\xCD\\";
constexpr float kDefaultMotionDelay = 4.0f;
constexpr float kDefaultMotionSpeedFactor = 1.0f;
constexpr float kMinimumMotionDelay = 1.0f / kActDelayUnitMs;
constexpr float kAttackMotionFactor = 0.0023148148f;
constexpr int kMoveStateId = 1;
constexpr int kAttackStateId = kGameActorAttackStateId;
constexpr int kDeathStateId = kGameActorDeathStateId;
constexpr int kSecondAttackStateId = 9;
constexpr int kHitReactionStateId = 4;
constexpr int kDeathAction = 64;
constexpr int kNonPcDeathAction = 32;
constexpr int kDeathMotion = 4;
constexpr int kHitReactionAction = 48;
constexpr int kNonPcHitReactionAction = 24;
constexpr int kHitReactionMotion = 5;
constexpr u32 kHitReactionDurationMs = 200;
constexpr float kHitReactionKnockbackDist = 6.0f;
constexpr int kMonsterJobMin = 1000;
constexpr int kMercenaryJobMin = 6001;
constexpr int kMercenaryJobMax = 6047;
constexpr float kItemBillboardDepthBias = 0.00015f;
constexpr float kItemProjectNearPlane = 10.0f;
constexpr float kItemProjectSubmitNearPlane = 80.0f;
constexpr const char* kLegacyItemSpriteRoot = "data\\sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\";

bool ComputeBillboardMotionBounds(CSprRes* sprRes, const CMotion* motion,
    const CMotion* next, float fraction, const ActorBillboardAnimation& animation, RECT* bounds)
{
    if (!animation.enabled && !animation.sharpPixels) {
        return ComputeMotionClipBounds(sprRes, motion, bounds);
    }

    bool hasBounds = ComputeInterpolatedMotionBounds(sprRes, motion,
        animation.enabled ? next : motion, animation.enabled ? fraction : 0.0f, bounds);
    if (!animation.enabled && animation.sharpPixels) {
        // Sharp static poses use the transformed sampler at 2x. Large surfaces
        // fall back to the legacy blitter at 1x, so cover either raster path.
        RECT legacyBounds{};
        if (ComputeMotionClipBounds(sprRes, motion, &legacyBounds)) {
            if (hasBounds) {
                bounds->left = (std::min)(bounds->left, legacyBounds.left);
                bounds->top = (std::min)(bounds->top, legacyBounds.top);
                bounds->right = (std::max)(bounds->right, legacyBounds.right);
                bounds->bottom = (std::max)(bounds->bottom, legacyBounds.bottom);
            } else {
                *bounds = legacyBounds;
            }
            hasBounds = true;
        }
    }
    return hasBounds;
}

struct BillboardComposeSurface {
    BillboardComposeSurface(int width, int height, bool sharpPixels = false)
        : m_scale(sharpPixels && width <= 1024 && height <= 1024 ? 2 : 1),
          m_width(width * m_scale), m_height(height * m_scale),
          m_pixels(static_cast<size_t>(m_width) * static_cast<size_t>(m_height), 0u)
    {
        animation.sharpPixels = sharpPixels;
    }

    // Reuse the drawing code to measure every layer and attachment without
    // allocating pixels or estimating optical flow. Rasterize after sizing.
    explicit BillboardComposeSurface(const ActorBillboardAnimation& sample)
        : animation(sample), m_boundsOnly(true)
    {
    }

    void Clear(unsigned int color)
    {
        std::fill(m_pixels.begin(), m_pixels.end(), color);
    }

    void BltSprite(int x, int y, CSprRes* sprRes, const CMotion* motion, unsigned int* palette)
    {
        if (m_boundsOnly) {
            IncludeSpriteBounds(static_cast<float>(x), static_cast<float>(y), sprRes, motion, motion, 0.0f);
            return;
        }
        if (m_scale > 1) {
            BlitInterpolatedMotionToArgb(m_pixels.data(), m_width, m_height,
                static_cast<float>(x), static_cast<float>(y), sprRes, motion, motion,
                palette, 0.0f, animation.sharpPixels, m_scale, animation.sharpness);
        } else {
            BlitMotionToArgb(m_pixels.data(), m_width, m_height, x, y, sprRes, motion, palette);
        }
    }

    void BltInterpolatedSprite(float x, float y, CSprRes* sprRes, const CMotion* motion,
        const CMotion* next, unsigned int* palette, float fraction)
    {
        if (m_boundsOnly) {
            if (!std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > 1000000 || std::fabs(y) > 1000000) {
                return;
            }
            IncludeSpriteBounds(x, y, sprRes, motion, next, fraction);
            return;
        }
        if (animation.enabled) {
            BlitInterpolatedMotionToArgb(m_pixels.data(), m_width, m_height,
                x, y, sprRes, motion, next, palette, fraction,
                animation.sharpPixels, m_scale, animation.sharpness);
        } else {
            BltSprite(static_cast<int>(x), static_cast<int>(y), sprRes, motion, palette);
        }
    }

    ActorBillboardAnimation animation;

    unsigned int* GetImageData() { return m_pixels.data(); }
    const unsigned int* GetImageData() const { return m_pixels.data(); }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetScale() const { return m_scale; }

    bool GetMotionBounds(RECT* bounds) const
    {
        if (!bounds || !m_hasBounds) {
            return false;
        }
        *bounds = m_bounds;
        return true;
    }

    void ToLogicalBounds(tagRECT* bounds) const
    {
        // Geometry, anchors and hit testing stay in the original sprite pixels.
        // Right/bottom are exclusive, so round them outward when reducing.
        bounds->left /= m_scale;
        bounds->top /= m_scale;
        bounds->right = (bounds->right + m_scale - 1) / m_scale;
        bounds->bottom = (bounds->bottom + m_scale - 1) / m_scale;
    }

private:
    void IncludeSpriteBounds(float x, float y, CSprRes* sprRes, const CMotion* motion,
        const CMotion* next, float fraction)
    {
        RECT bounds{};
        if (!ComputeBillboardMotionBounds(sprRes, motion, next, fraction, animation, &bounds)) {
            return;
        }
        // Round fractional attachments outward. This also covers static drawing
        // truncating the origin after the final composition anchor is added.
        bounds.left = static_cast<LONG>(std::floor(x + bounds.left));
        bounds.top = static_cast<LONG>(std::floor(y + bounds.top));
        bounds.right = static_cast<LONG>(std::ceil(x + bounds.right));
        bounds.bottom = static_cast<LONG>(std::ceil(y + bounds.bottom));
        if (m_hasBounds) {
            m_bounds.left = (std::min)(m_bounds.left, bounds.left);
            m_bounds.top = (std::min)(m_bounds.top, bounds.top);
            m_bounds.right = (std::max)(m_bounds.right, bounds.right);
            m_bounds.bottom = (std::max)(m_bounds.bottom, bounds.bottom);
        } else {
            m_bounds = bounds;
        }
        m_hasBounds = true;
    }

    int m_scale = 1;
    int m_width = 0;
    int m_height = 0;
    std::vector<unsigned int> m_pixels;
    bool m_boundsOnly = false;
    bool m_hasBounds = false;
    RECT m_bounds{};
};

#define LOG_ACT_LOAD(...) do { if constexpr (kLogActLoad) { DbgLog(__VA_ARGS__); } } while (0)

float NormalizeAngle360(float angle)
{
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

int ResolveEightDirectionFromLongitude(float longitude, bool useRoundedDirs)
{
    longitude = NormalizeAngle360(longitude);
    if (useRoundedDirs) {
        if (longitude < 22.5f || longitude > 337.5f) {
            return 0;
        }
        return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
    }

    return static_cast<int>(longitude / 45.0f) & 7;
}

float ResolveRuntimeActorLongitudeForDirection(float rotationDegrees)
{
    return NormalizeAngle360(-g_runtimeActorCameraLongitude - rotationDegrees);
}

bool ShouldUseRoundedDirectionForActor(const CGameActor* actor)
{
    if (!actor) {
        return false;
    }

    if (actor->m_isPc != 0) {
        return actor->m_baseAction == 0 || actor->m_baseAction == 8 || actor->m_baseAction == 16;
    }

    if (actor->m_job > 45 && actor->m_job < 1000 && actor->m_baseAction == 0) {
        return true;
    }

    if (actor->m_job >= kMercenaryJobMin && actor->m_job <= kMercenaryJobMax) {
        return actor->m_baseAction == 0 || actor->m_baseAction == 8 || actor->m_baseAction == 16;
    }

    return false;
}

bool TryResolveNonPcSpritePaths(const char* spriteRoot, const char* jobName, char* actPath, char* sprPath)
{
    if (!spriteRoot || !jobName || !*jobName || !actPath || !sprPath) {
        return false;
    }

    std::sprintf(actPath, "%s%s.act", spriteRoot, jobName);
    std::sprintf(sprPath, "%s%s.spr", spriteRoot, jobName);
    return g_resMgr.IsExist(actPath) && g_resMgr.IsExist(sprPath);
}

const char* ResolveNonPcSpriteAlias(int job, const char* jobName)
{
    switch (job) {
    case 0x3F8: // JT_ARCHER_SKELETON
    case 0x58C: // JT_G_ARCHER_SKELETON
        return "skel_archer";
    case 0x404: // JT_SOLDIER_SKELETON
    case 0x61A: // JT_G_SOLDIER_SKELETON
        return "skel_soldier";
    default:
        return jobName;
    }
}

float ActorRotationDegreesFromDir(int dir)
{
    return NormalizeAngle360(45.0f * static_cast<float>(dir & 7));
}

bool IsDualWeaponPcJob(int job)
{
    return job == 12 || job == 4013 || job == 4035;
}

int ResolvePcAttackWeaponValue(const CGameActor& actor)
{
    const CPc* pcActor = dynamic_cast<const CPc*>(&actor);
    const bool isPc = actor.m_isPc != 0 && pcActor != nullptr;
    if (!isPc) {
        return 0;
    }

    int weaponValue = pcActor->m_weapon & 0xFFFF;

    if (IsDualWeaponPcJob(actor.m_job) && pcActor->m_shield != 0) {
        weaponValue = (pcActor->m_weapon & 0xFFFF) | ((pcActor->m_shield & 0xFFFF) << 16);
    }

    const bool isLocalPlayer = actor.m_gid != 0
        && (actor.m_gid == g_session.m_gid || actor.m_gid == g_session.m_aid);
    if (isLocalPlayer) {
        const int localWeaponValue = g_session.GetCurrentPlayerWeaponValue();
        if (localWeaponValue != 0) {
            return localWeaponValue;
        }
    }

    return weaponValue;
}

int ResolvePcAttackAction(const CGameActor& actor)
{
    const int weaponValue = ResolvePcAttackWeaponValue(actor);
    if (g_session.ResolvePackedWeaponType(actor.m_job, weaponValue) == 0) {
        return 40; // Bare-handed attack; 80/88 are weapon attack families.
    }
    return g_session.IsSecondAttack(actor.m_job, actor.m_sex, weaponValue) ? 88 : 80;
}

bool IsMonsterLikeWaveActor(const CGameActor& actor)
{
    return actor.m_job >= kMonsterJobMin
        && (actor.m_job < kMercenaryJobMin || actor.m_job > kMercenaryJobMax);
}

bool ShouldPlayMotionWaveForActor(const CGameActor& actor)
{
    return actor.m_isPc != 0 || IsMonsterLikeWaveActor(actor);
}

u32 ResolveClientInfoActorAid(const CGameActor& actor)
{
    if (actor.m_gid != 0 && (actor.m_gid == g_session.m_gid || actor.m_gid == g_session.m_aid)) {
        return g_session.m_aid != 0 ? g_session.m_aid : actor.m_gid;
    }

    if (const CPc* pc = dynamic_cast<const CPc*>(&actor)) {
        return pc->m_gid;
    }
    return actor.m_gid;
}

bool ShouldUseGameMasterAppearance(const CGameActor& actor)
{
    return actor.m_isPc != 0 && IsGravityAid(ResolveClientInfoActorAid(actor));
}

int ResolveDisplayJob(const CGameActor& actor)
{
    return ShouldUseGameMasterAppearance(actor) ? JT_G_MASTER : actor.m_job;
}

std::string NormalizeWaveEventPath(const char* eventName);

bool QueueActorHitWave(CGameActor& actor, const char* eventName)
{
    if (!eventName || !*eventName) {
        return false;
    }

    const std::string normalized = NormalizeWaveEventPath(eventName);
    if (normalized.empty()) {
        return false;
    }

    std::snprintf(actor.m_hitWaveName, sizeof(actor.m_hitWaveName), "%s", normalized.c_str());
    actor.m_isPlayHitWave = 1;
    return true;
}

CGameActor* ResolveActorByGidForCastRetarget(u32 gid)
{
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || !gameMode->m_world) {
        return nullptr;
    }

    CWorld* world = gameMode->m_world;
    if (gid != 0 && (gid == g_session.m_aid || gid == g_session.m_gid)) {
        return world->m_player;
    }

    for (CGameActor* actor : world->m_actorList) {
        if (actor && actor->m_gid == gid) {
            return actor;
        }
    }

    const auto it = gameMode->m_runtimeActors.find(gid);
    if (it != gameMode->m_runtimeActors.end()) {
        return it->second;
    }

    return nullptr;
}

bool TryPlayQueuedWaveAtActor(const CGameActor& actor, const char* waveName)
{
    if (!waveName || !*waveName) {
        return false;
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::string normalized = NormalizeWaveEventPath(waveName);
    if (normalized.empty()) {
        return false;
    }

    std::array<std::string, 3> candidates = {
        normalized,
        std::string("wav\\") + normalized,
        std::string("data\\wav\\") + normalized,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(), actor.m_pos, listenerPos)) {
            return true;
        }
    }

    return false;
}

bool ContainsAsciiCaseInsensitive(const char* text, const char* token)
{
    if (!text || !token || !*text || !*token) {
        return false;
    }

    std::string loweredText(text);
    std::string loweredToken(token);
    std::transform(loweredText.begin(), loweredText.end(), loweredText.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(loweredToken.begin(), loweredToken.end(), loweredToken.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return loweredText.find(loweredToken) != std::string::npos;
}

std::string NormalizeWaveEventPath(const char* eventName)
{
    std::string normalized = eventName ? eventName : "";
    std::replace(normalized.begin(), normalized.end(), '/', '\\');
    return normalized;
}

bool TryPlayActorMotionWave(CGameActor& actor, const char* eventName)
{
    if (!ShouldPlayMotionWaveForActor(actor) || !ContainsAsciiCaseInsensitive(eventName, ".wav")) {
        return false;
    }

    if (actor.m_isPc != 0) {
        return QueueActorHitWave(actor, eventName);
    }

    CAudio* audio = CAudio::GetInstance();
    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!audio || !gameMode || !gameMode->m_world || !gameMode->m_world->m_player) {
        return false;
    }

    const vector3d listenerPos = gameMode->m_world->m_player->m_pos;
    const std::string normalized = NormalizeWaveEventPath(eventName);
    if (normalized.empty()) {
        return false;
    }

    std::array<std::string, 3> candidates = {
        normalized,
        std::string("wav\\") + normalized,
        std::string("data\\wav\\") + normalized,
    };

    for (const std::string& candidate : candidates) {
        if (audio->PlaySound3D(candidate.c_str(), actor.m_pos, listenerPos)) {
            return true;
        }
    }

    return false;
}

void PlayMotionWaveEvents(CRenderObject* object, CActRes* actRes, int action, int oldMotion, int newMotion)
{
    if (!object || !actRes || newMotion == oldMotion) {
        return;
    }

    CGameActor* actor = dynamic_cast<CGameActor*>(object);
    if (!actor) {
        return;
    }

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        return;
    }

    auto playRange = [&](int first, int last) {
        for (int motionIndex = first; motionIndex <= last; ++motionIndex) {
            const CMotion* motion = actRes->GetMotion(action, motionIndex);
            if (!motion || motion->eventId < 0) {
                continue;
            }

            const char* eventName = actRes->GetEventName(motion->eventId);
            if (!eventName || !*eventName) {
                continue;
            }

            TryPlayActorMotionWave(*actor, eventName);
        }
    };

    if (newMotion > oldMotion) {
        playRange(oldMotion + 1, newMotion);
        return;
    }

    if (oldMotion + 1 < motionCount) {
        playRange(oldMotion + 1, motionCount - 1);
    }
    playRange(0, newMotion);
}

void ProcessRenderMotionWaveEvents(CRenderObject* object, CActRes* actRes, int action, int motion)
{
    if (!object || !actRes) {
        return;
    }

    const int motionWaveBaseAction = object->m_baseAction;

    const int motionCount = actRes->GetMotionCount(action);
    if (motionCount <= 0) {
        object->m_oldBaseAction = motionWaveBaseAction;
        object->m_oldMotion = 0;
        return;
    }

    const int clampedMotion = (std::max)(0, (std::min)(motion, motionCount - 1));
    if (object->m_oldBaseAction == motionWaveBaseAction && object->m_oldMotion == clampedMotion) {
        return;
    }

    int previousMotion = object->m_oldMotion;
    if (object->m_oldBaseAction != motionWaveBaseAction) {
        previousMotion = -1;
    }
    previousMotion = (std::max)(-1, (std::min)(previousMotion, motionCount - 1));

    PlayMotionWaveEvents(object, actRes, action, previousMotion, clampedMotion);
    object->m_oldBaseAction = motionWaveBaseAction;
    object->m_oldMotion = clampedMotion;
}

void ProcessQueuedHitWave(CGameActor& actor)
{
    if (actor.m_isPlayHitWave == 0 || actor.m_hitWaveName[0] == '\0') {
        return;
    }

    actor.m_isPlayHitWave = 0;
    TryPlayQueuedWaveAtActor(actor, actor.m_hitWaveName);

    actor.m_hitWaveName[0] = '\0';
}

void ApplyQueuedHitReaction(CGameActor& actor, const WBA& hitInfo)
{
    CGameActor* sourceActor = nullptr;
    if (hitInfo.gid != 0) {
        if (g_world.m_player && g_world.m_player->m_gid == hitInfo.gid) {
            sourceActor = g_world.m_player;
        } else {
            for (CGameActor* entry : g_world.m_actorList) {
                if (entry && entry->m_gid == hitInfo.gid) {
                    sourceActor = entry;
                    break;
                }
            }
        }
    }

    actor.m_damageDestX = hitInfo.damageDestX;
    actor.m_damageDestZ = hitInfo.damageDestZ;

    if (hitInfo.message == 94) {
        actor.SendMsg(nullptr, 95, 0, 0, 0);
        return;
    }

    if (hitInfo.damage == 0) {
        if (sourceActor) {
            sourceActor->SendMsg(&actor, 89, 0, 0, 0);
        } else {
            actor.SendMsg(nullptr, 89, 0, 0, 0);
        }
        return;
    }

    if (hitInfo.playHitSound) {
        if (hitInfo.waveName[0] != '\0') {
            TryPlayQueuedWaveAtActor(actor, hitInfo.waveName);
        } else if (const char* genericWave = g_session.GetWeaponHitWaveName(-1)) {
            TryPlayQueuedWaveAtActor(actor, genericWave);
        }
    }

    if (actor.m_stateId == kDeathStateId) {
        return;
    }

    if (hitInfo.damage != 0 && hitInfo.damageKind != 0) {
        actor.SendMsg(sourceActor,
            88,
            hitInfo.damage,
            static_cast<int>(hitInfo.damageColor),
            hitInfo.damageKind);

        if (hitInfo.message == 93) {
            actor.SendMsg(nullptr, 92, 0, 0, 0);
        }
    }

    // Protected allies keep their current action. Preserve the queued hit's
    // sound, numbers, and critical visual before suppressing its motion.
    if (std::any_of(g_world.m_gameObjectList.begin(), g_world.m_gameObjectList.end(),
        [&actor](const CGameObject* object) {
            const auto* effect = dynamic_cast<const CRagEffect*>(object);
            return effect && effect->IsDevotionLinkTo(&actor);
        })) return;

    actor.SetState(kHitReactionStateId);
    if (actor.m_stateId == kHitReactionStateId && hitInfo.attackedMotionTime > 0) {
        const int frames = actor.m_actRes
            ? (std::max)(1, actor.m_actRes->GetMotionCount(actor.m_curAction)) : 1;
        actor.m_motionSpeed = (std::max)(kMinimumMotionDelay,
            static_cast<float>(hitInfo.attackedMotionTime) / (static_cast<float>(frames) * kActDelayUnitMs));
    }
}

bool IsPortalFallbackJob(int job)
{
    return job == kJobWarpNpc
        || job == kJobWarpPortal
        || job == kJobPreWarpPortal;
}

unsigned int PremultiplyArgb(unsigned int color)
{
    const unsigned int alpha = (color >> 24) & 0xFFu;
    if (alpha == 0u || alpha == 0xFFu) {
        return color;
    }

    const unsigned int red = ((color >> 16) & 0xFFu) * alpha / 255u;
    const unsigned int green = ((color >> 8) & 0xFFu) * alpha / 255u;
    const unsigned int blue = (color & 0xFFu) * alpha / 255u;
    return (alpha << 24) | (red << 16) | (green << 8) | blue;
}

void AlphaBlendArgb(unsigned int& dst, unsigned int src)
{
    const unsigned int srcA = (src >> 24) & 0xFFu;
    if (srcA == 0u) {
        return;
    }
    if (srcA == 0xFFu) {
        dst = src;
        return;
    }

    const unsigned int dstA = (dst >> 24) & 0xFFu;
    const unsigned int srcR = (src >> 16) & 0xFFu;
    const unsigned int srcG = (src >> 8) & 0xFFu;
    const unsigned int srcB = src & 0xFFu;
    const unsigned int dstR = (dst >> 16) & 0xFFu;
    const unsigned int dstG = (dst >> 8) & 0xFFu;
    const unsigned int dstB = dst & 0xFFu;

    const unsigned int invA = 255u - srcA;
    const unsigned int outA = srcA + (dstA * invA) / 255u;
    const unsigned int outR = srcR + (dstR * invA) / 255u;
    const unsigned int outG = srcG + (dstG * invA) / 255u;
    const unsigned int outB = srcB + (dstB * invA) / 255u;
    dst = (outA << 24) | (outR << 16) | (outG << 8) | outB;
}

void DrawFilledCircle(unsigned int* pixels, int width, int height, float centerX, float centerY, float radius, unsigned int color)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0.0f) {
        return;
    }

    const int minX = (std::max)(0, static_cast<int>(std::floor(centerX - radius - 1.0f)));
    const int maxX = (std::min)(width - 1, static_cast<int>(std::ceil(centerX + radius + 1.0f)));
    const int minY = (std::max)(0, static_cast<int>(std::floor(centerY - radius - 1.0f)));
    const int maxY = (std::min)(height - 1, static_cast<int>(std::ceil(centerY + radius + 1.0f)));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - centerX;
            const float dy = (static_cast<float>(y) + 0.5f) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist > radius) {
                continue;
            }

            const float coverage = 1.0f - dist / radius;
            const unsigned int scaledAlpha = static_cast<unsigned int>(((color >> 24) & 0xFFu) * coverage);
            if (scaledAlpha == 0u) {
                continue;
            }

            const unsigned int scaledColor = PremultiplyArgb(
                (scaledAlpha << 24) | (color & 0x00FFFFFFu));
            AlphaBlendArgb(pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)], scaledColor);
        }
    }
}

void DrawRing(unsigned int* pixels, int width, int height, float centerX, float centerY, float radius, float thickness, unsigned int color)
{
    if (!pixels || width <= 0 || height <= 0 || radius <= 0.0f || thickness <= 0.0f) {
        return;
    }

    const float innerRadius = (std::max)(0.0f, radius - thickness * 0.5f);
    const float outerRadius = radius + thickness * 0.5f;
    const int minX = (std::max)(0, static_cast<int>(std::floor(centerX - outerRadius - 1.0f)));
    const int maxX = (std::min)(width - 1, static_cast<int>(std::ceil(centerX + outerRadius + 1.0f)));
    const int minY = (std::max)(0, static_cast<int>(std::floor(centerY - outerRadius - 1.0f)));
    const int maxY = (std::min)(height - 1, static_cast<int>(std::ceil(centerY + outerRadius + 1.0f)));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - centerX;
            const float dy = (static_cast<float>(y) + 0.5f) - centerY;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist < innerRadius || dist > outerRadius) {
                continue;
            }

            const float distanceToBand = std::fabs(dist - radius);
            const float coverage = 1.0f - distanceToBand / ((std::max)(0.001f, thickness * 0.5f));
            const unsigned int scaledAlpha = static_cast<unsigned int>(((color >> 24) & 0xFFu) * coverage);
            if (scaledAlpha == 0u) {
                continue;
            }

            const unsigned int scaledColor = PremultiplyArgb(
                (scaledAlpha << 24) | (color & 0x00FFFFFFu));
            AlphaBlendArgb(pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)], scaledColor);
        }
    }
}

bool DrawWarpPortalFallback(BillboardComposeSurface& bitmap, const CPc& actor, int* outJob)
{
    if (actor.m_job == kJobHiddenWarpNpc || !IsPortalFallbackJob(actor.m_job)) {
        return false;
    }

    unsigned int* pixels = bitmap.GetImageData();
    const int width = static_cast<int>(bitmap.GetWidth());
    const int height = static_cast<int>(bitmap.GetHeight());
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    const bool preWarp = actor.m_job == kJobPreWarpPortal;
    const unsigned int ringColor = preWarp ? 0xD0FF9A52u : 0xD05AC8FFu;
    const unsigned int glowColor = preWarp ? 0x60FFB874u : 0x605AC8FFu;
    const unsigned int coreColor = preWarp ? 0x409C5A20u : 0x403088D0u;
    const float centerX = static_cast<float>(width) * 0.5f;
    const float centerY = static_cast<float>(height) * 0.76f;

    const float scale = static_cast<float>(bitmap.GetScale());
    DrawFilledCircle(pixels, width, height, centerX, centerY - 2.0f * scale, 20.0f * scale, coreColor);
    DrawRing(pixels, width, height, centerX, centerY, 28.0f * scale, 5.0f * scale, glowColor);
    DrawRing(pixels, width, height, centerX, centerY, 24.0f * scale, 3.0f * scale, ringColor);
    DrawRing(pixels, width, height, centerX, centerY, 16.0f * scale, 2.0f * scale, 0x90FFFFFFu);

    for (int band = 0; band < 4; ++band) {
        const float offset = (-12.0f + 8.0f * static_cast<float>(band)) * scale;
        DrawFilledCircle(pixels, width, height, centerX + offset * 0.35f, centerY - 18.0f * scale - std::fabs(offset) * 0.4f, 5.5f * scale, glowColor);
        DrawFilledCircle(pixels, width, height, centerX + offset * 0.25f, centerY - 32.0f * scale - std::fabs(offset) * 0.8f, 3.5f * scale, 0x50FFFFFFu);
    }

    if (outJob) {
        *outJob = actor.m_job;
    }
    return true;
}

int ResolveDirFromRotationDegrees(float rotationDegrees)
{
    return static_cast<int>((NormalizeAngle360(rotationDegrees) + 22.5f) / 45.0f) & 7;
}

float TileToWorldCoordX(const CWorld* world, int tileX)
{
    const int width = world && world->m_attr ? world->m_attr->m_width : (world && world->m_ground ? world->m_ground->m_width : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileX) - static_cast<float>(width) * 0.5f) * zoom + zoom * 0.5f;
}

float TileToWorldCoordZ(const CWorld* world, int tileY)
{
    const int height = world && world->m_attr ? world->m_attr->m_height : (world && world->m_ground ? world->m_ground->m_height : 0);
    const float zoom = world && world->m_attr ? static_cast<float>(world->m_attr->m_zoom) : (world && world->m_ground ? world->m_ground->m_zoom : 5.0f);
    return (static_cast<float>(tileY) - static_cast<float>(height) * 0.5f) * zoom + zoom * 0.5f;
}

int ResolveDirFromStepDelta(int dx, int dy)
{
    if (dx == 0 && dy < 0) {
        return 0;
    }
    if (dx > 0 && dy < 0) {
        return 1;
    }
    if (dx > 0 && dy == 0) {
        return 2;
    }
    if (dx > 0 && dy > 0) {
        return 3;
    }
    if (dx == 0 && dy > 0) {
        return 4;
    }
    if (dx < 0 && dy > 0) {
        return 5;
    }
    if (dx < 0 && dy == 0) {
        return 6;
    }
    if (dx < 0 && dy < 0) {
        return 7;
    }
    return g_session.m_playerDir & 7;
}

int ClampDamageNumber(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 999999) {
        return 999999;
    }
    return value;
}

int CountDamageDigits(int value)
{
    if (value >= 100000) {
        return 6;
    }
    if (value >= 10000) {
        return 5;
    }
    if (value >= 1000) {
        return 4;
    }
    if (value >= 100) {
        return 3;
    }
    if (value >= 10) {
        return 2;
    }
    return 1;
}

CMsgEffect* SpawnDamageNumberEffect(CGameActor& actor, CGameActor* sourceActor, int value, u32 colorArgb, int kind)
{
    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return nullptr;
    }

    effect->m_colorArgb = colorArgb;
    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), kind, 0);
    effect->SendMsg(nullptr,
        64,
        reinterpret_cast<msgparam_t>(&actor.m_pos),
        value,
        0);
    effect->SendMsg(&actor, 50, 0, 0, 0);

    if (kind == kTotalDamageNumberKind) {
        effect->m_zoom = 3.0f;
        effect->m_orgZoom = 3.0f;
    }

    if (sourceActor) {
        const float dx = actor.m_pos.x - sourceActor->m_pos.x;
        const float dz = actor.m_pos.z - sourceActor->m_pos.z;
        const float lengthSq = dx * dx + dz * dz;
        if (lengthSq > 0.0001f) {
            const float invLength = 0.8f / std::sqrt(lengthSq);
            effect->m_destPos.x = dx * invLength;
            effect->m_destPos.z = dz * invLength;
        }
    }

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
    actor.m_lastDamageNumberEffect = effect;
    return effect;
}

void SpawnMissEffect(CGameActor& actor)
{
    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return;
    }

    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), 13, 0);
    effect->m_isVisible = 1;
    effect->m_stateStartTick = timeGetTime();
    effect->m_pos = actor.m_pos;
    effect->m_pos.y -= 12.0f;
    effect->m_orgPos = effect->m_pos;
    effect->m_zoom = 1.0f;
    effect->m_orgZoom = 1.0f;
    effect->m_colorArgb = 0xFFFFFFFFu;
    effect->m_spriteActionIndex = 0;
    effect->m_spriteMotionIndex = 0;
    effect->SendMsg(&actor, 50, 0, 0, 0);

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
}

void SpawnCriticalEffect(CGameActor& actor)
{
    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return;
    }

    CMsgEffect* anchorEffect = actor.m_lastDamageNumberEffect;

    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), 15, 0);
    effect->m_isVisible = 1;
    effect->m_stateStartTick = anchorEffect ? anchorEffect->m_stateStartTick : timeGetTime();
    effect->m_pos = anchorEffect ? anchorEffect->m_pos : actor.m_pos;
    if (!anchorEffect) {
        effect->m_pos.y -= 12.0f;
    }
    effect->m_orgPos = anchorEffect ? anchorEffect->m_orgPos : effect->m_pos;
    effect->m_destPos = anchorEffect ? anchorEffect->m_destPos : vector3d{};
    effect->m_destPos2 = anchorEffect ? anchorEffect->m_destPos2 : vector3d{};
    effect->m_zoom = 1.0f;
    effect->m_orgZoom = 1.0f;
    effect->m_colorArgb = 0xFFFFFFFFu;
    effect->m_spriteActionIndex = 2;
    effect->m_spriteMotionIndex = 0;
    effect->SendMsg(&actor, 50, 0, 0, 0);

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
    actor.m_lastDamageNumberEffect = nullptr;
}

void SpawnLuckyDodgeEffect(CGameActor& actor)
{
    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return;
    }

    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), 17, 0);
    effect->m_isVisible = 1;
    effect->m_stateStartTick = timeGetTime();
    effect->m_pos = actor.m_pos;
    effect->m_pos.y -= 12.0f;
    effect->m_orgPos = effect->m_pos;
    effect->m_zoom = 1.0f;
    effect->m_orgZoom = 1.0f;
    effect->m_colorArgb = 0xFFFFFFFFu;
    effect->m_spriteActionIndex = 3;
    effect->m_spriteMotionIndex = 0;
    effect->SendMsg(&actor, 50, 0, 0, 0);

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
}

constexpr int kAttachedActorSpriteMsgEffectType = 203;
constexpr const char* kEffectSpriteRoot = "data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\";

const char* ResolveCartEffectStem(int effectType)
{
    switch (effectType) {
    case 1:
        return "\xBC\xD5\xBC\xF6\xB7\xB9";
    case 0x10:
        return "\xBC\xD5\xBC\xF6\xB7\xB9" "1";
    case 0x11:
        return "\xBC\xD5\xBC\xF6\xB7\xB9" "2";
    case 0x12:
        return "\xBC\xD5\xBC\xF6\xB7\xB9" "3";
    case 0x13:
        return "\xBC\xD5\xBC\xF6\xB7\xB9" "4";
    case 0x15:
        return "\xBD\xB4\xB3\xEB\xBC\xD5\xBC\xF6\xB7\xB9";
    case 0x19:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBC\xD5\xBC\xF6\xB7\xB9";
    case 0x1A:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBC\xD5\xBC\xF6\xB7\xB9" "1";
    case 0x1B:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBC\xD5\xBC\xF6\xB7\xB9" "2";
    case 0x1C:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBC\xD5\xBC\xF6\xB7\xB9" "3";
    case 0x1D:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBC\xD5\xBC\xF6\xB7\xB9" "4";
    case 0x1E:
        return "\xBA\xA3\xC0\xCC\xBA\xF1\xBD\xB4\xB3\xEB\xBC\xD5\xBC\xF6\xB7\xB9";
    default:
        return nullptr;
    }
}

std::string BuildCartEffectPath(int effectType, const char* extension)
{
    const char* stem = ResolveCartEffectStem(effectType);
    if (!stem || !extension) {
        return {};
    }

    std::string path = kEffectSpriteRoot;
    path += stem;
    path += extension;
    return path;
}

bool IsLiveMsgEffectObject(const CMsgEffect* effect)
{
    if (!effect) {
        return false;
    }

    const auto it = std::find(g_world.m_gameObjectList.begin(), g_world.m_gameObjectList.end(),
        static_cast<const CGameObject*>(effect));
    return it != g_world.m_gameObjectList.end();
}

void DetachOwnedMsgEffect(CGameActor& actor, CMsgEffect* effect)
{
    if (!effect) {
        return;
    }

    actor.DeleteMatchingEffect(effect);
    effect->SendMsg(&actor, 49, 0, 0, 0);
    effect->SendMsg(&actor, 53, 0, 0, 0);
}

void DestroyAttachedMsgEffect(CGameActor& actor, CMsgEffect** effectSlot)
{
    if (!effectSlot || !*effectSlot) {
        return;
    }

    CMsgEffect* effect = *effectSlot;
    DetachOwnedMsgEffect(actor, effect);
    *effectSlot = nullptr;
}

void SpawnCartEffect(CGameActor& actor, int effectType)
{
    const std::string actPath = BuildCartEffectPath(effectType, ".act");
    const std::string sprPath = BuildCartEffectPath(effectType, ".spr");
    if (actPath.empty() || sprPath.empty()) {
        return;
    }

    CActRes* actRes = g_resMgr.GetAs<CActRes>(actPath.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprPath.c_str());
    if (!actRes || !sprRes) {
        DbgLog("[CartEffect] resource load failed gid=%u effectType=%d spr='%s' act='%s'\n",
            static_cast<unsigned int>(actor.m_gid),
            effectType,
            sprPath.c_str(),
            actPath.c_str());
        return;
    }

    DestroyAttachedMsgEffect(actor, &actor.m_cartEffect);

    CMsgEffect* effect = new CMsgEffect();
    if (!effect) {
        return;
    }

    effect->SendMsg(nullptr, 22, static_cast<int>(actor.m_gid), kAttachedActorSpriteMsgEffectType, 0);
    effect->m_isVisible = 1;
    effect->m_stateStartTick = timeGetTime();
    effect->m_pos = actor.m_pos;
    effect->m_orgPos = actor.m_pos;
    effect->m_zoom = 1.0f;
    effect->m_orgZoom = 1.0f;
    effect->m_colorArgb = 0xFFFFFFFFu;
    effect->m_spriteActionIndex = 0;
    effect->m_spriteMotionIndex = 0;
    effect->m_customSpritePath = sprPath;
    effect->m_customActPath = actPath;
    effect->SendMsg(&actor, 50, 0, 0, 0);

    g_world.m_gameObjectList.push_back(effect);
    actor.m_msgEffectList.push_back(effect);
    actor.m_cartEffect = effect;
    DbgLog("[CartEffect] spawn gid=%u effect=%p type=%d pos=(%.2f,%.2f,%.2f) spr='%s' act='%s'\n",
        static_cast<unsigned int>(actor.m_gid),
        static_cast<void*>(effect),
        effectType,
        actor.m_pos.x,
        actor.m_pos.y,
        actor.m_pos.z,
        sprPath.c_str(),
        actPath.c_str());
}

bool FindActivePathSegment(const CPathInfo& path, u32 now, size_t* outStartIndex)
{
    if (path.m_cells.size() < 2 || !outStartIndex) {
        return false;
    }

    for (size_t index = 1; index < path.m_cells.size(); ++index) {
        if (now < path.m_cells[index].arrivalTime) {
            *outStartIndex = index - 1;
            return true;
        }
    }

    *outStartIndex = path.m_cells.size() - 2;
    return true;
}

bool InterpolatePathPosition(const CGameActor& actor, u32 now, vector3d* outPos)
{
    if (!outPos || actor.m_path.m_cells.empty()) {
        return false;
    }

    const CWorld* world = &g_world;
    if (actor.m_path.m_cells.size() == 1) {
        const PathCell& cell = actor.m_path.m_cells.front();
        outPos->x = TileToWorldCoordX(world, cell.x);
        outPos->z = TileToWorldCoordZ(world, cell.y);
        outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
        return true;
    }

    if (now >= actor.m_path.m_cells.back().arrivalTime) {
        const PathCell& cell = actor.m_path.m_cells.back();
        outPos->x = TileToWorldCoordX(world, cell.x);
        outPos->z = TileToWorldCoordZ(world, cell.y);
        outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
        return true;
    }

    size_t startIndex = 0;
    if (!FindActivePathSegment(actor.m_path, now, &startIndex)) {
        return false;
    }

    const PathCell& startCell = actor.m_path.m_cells[startIndex];
    const PathCell& endCell = actor.m_path.m_cells[startIndex + 1];
    const u32 startTime = startCell.arrivalTime;
    const u32 endTime = endCell.arrivalTime;
    const float duration = static_cast<float>((std::max)(1u, endTime - startTime));
    const float ratio = static_cast<float>(now - startTime) / duration;
    const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));

    const float startX = startIndex == 0 ? actor.m_moveStartPos.x : TileToWorldCoordX(world, startCell.x);
    const float startZ = startIndex == 0 ? actor.m_moveStartPos.z : TileToWorldCoordZ(world, startCell.y);
    const float endX = TileToWorldCoordX(world, endCell.x);
    const float endZ = TileToWorldCoordZ(world, endCell.y);

    outPos->x = startX + (endX - startX) * clamped;
    outPos->z = startZ + (endZ - startZ) * clamped;
    outPos->y = world->m_attr ? world->m_attr->GetHeight(outPos->x, outPos->z) : 0.0f;
    return true;
}

int ResolvePcFacingDir(const CPc* actor)
{
    if (!actor) {
        return g_session.m_playerDir & 7;
    }

    if (std::isfinite(actor->m_roty)) {
        return const_cast<CPc*>(actor)->Get8Dir(actor->m_roty);
    }

    return g_session.m_playerDir & 7;
}

struct SharedNonPcBillboardKey {
    int job = 0;
    int action = 0;
    int motion = 0;
    ActorBillboardAnimation animation;

    bool operator<(const SharedNonPcBillboardKey& other) const
    {
        if (job != other.job) {
            return job < other.job;
        }
        if (action != other.action) {
            return action < other.action;
        }
        if (motion != other.motion) {
            return motion < other.motion;
        }
        return animation < other.animation;
    }
};

struct SharedNonPcBillboardValue {
    std::shared_ptr<CTexture> texture;
    u64 lastUsed = 0;
    tagRECT opaqueBounds{};
    int width = 0;
    int height = 0;
    int anchorX = kPlayerBillboardAnchorX;
    int anchorY = kPlayerBillboardAnchorY;
};

u64 NextBillboardCacheUse()
{
    static u64 use = 0;
    return ++use;
}

template<typename Key>
void TrimSharedBillboardCache(std::map<Key, SharedNonPcBillboardValue>& cache)
{
    size_t bytes = 0;
    u64 newestUse = 0;
    for (const auto& entry : cache) {
        if (entry.second.texture) {
            bytes += static_cast<size_t>(entry.second.texture->m_w)
                * entry.second.texture->m_h * sizeof(unsigned int);
        }
        newestUse = (std::max)(newestUse, entry.second.lastUsed);
    }
    while (bytes > kSharedBillboardCacheBudget) {
        auto oldest = cache.end();
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            // Actors retain shared ownership of displayed poses. Never evict a
            // live texture, or the new pose before its caller can acquire it.
            if (it->second.texture.use_count() == 1 && it->second.lastUsed < newestUse
                && (oldest == cache.end() || it->second.lastUsed < oldest->second.lastUsed)) {
                oldest = it;
            }
        }
        if (oldest == cache.end()) {
            break;
        }
        bytes -= static_cast<size_t>(oldest->second.texture->m_w)
            * oldest->second.texture->m_h * sizeof(unsigned int);
        cache.erase(oldest);
    }
}

std::map<SharedNonPcBillboardKey, SharedNonPcBillboardValue>& GetSharedNonPcBillboardCache()
{
    static std::map<SharedNonPcBillboardKey, SharedNonPcBillboardValue> cache;
    return cache;
}

struct SharedPlayerBillboardKey {
    int job = 0;
    int sex = 0;
    int head = 0;
    int bodyPalette = 0;
    int bodyStyle = 0;
    int headPalette = 0;
    int accessoryBottom = 0;
    int accessoryMid = 0;
    int accessoryTop = 0;
    int weapon = 0;
    int shield = 0;
    int action = 0;
    int motion = 0;
    std::array<int, 3> accessoryMotions{};
    ActorBillboardAnimation animation;

    bool operator<(const SharedPlayerBillboardKey& other) const
    {
        if (job != other.job) {
            return job < other.job;
        }
        if (sex != other.sex) {
            return sex < other.sex;
        }
        if (head != other.head) {
            return head < other.head;
        }
        if (bodyPalette != other.bodyPalette) {
            return bodyPalette < other.bodyPalette;
        }
        if (bodyStyle != other.bodyStyle) {
            return bodyStyle < other.bodyStyle;
        }
        if (headPalette != other.headPalette) {
            return headPalette < other.headPalette;
        }
        if (accessoryBottom != other.accessoryBottom) {
            return accessoryBottom < other.accessoryBottom;
        }
        if (accessoryMid != other.accessoryMid) {
            return accessoryMid < other.accessoryMid;
        }
        if (accessoryTop != other.accessoryTop) {
            return accessoryTop < other.accessoryTop;
        }
        if (weapon != other.weapon) {
            return weapon < other.weapon;
        }
        if (shield != other.shield) {
            return shield < other.shield;
        }
        if (action != other.action) {
            return action < other.action;
        }
        if (motion != other.motion) {
            return motion < other.motion;
        }
        if (accessoryMotions != other.accessoryMotions) {
            return accessoryMotions < other.accessoryMotions;
        }
        return animation < other.animation;
    }
};

using SharedPlayerBillboardValue = SharedNonPcBillboardValue;

bool ResolveCachedNonPcResourcesForActor(CPc& actor, CActRes** outActRes, CSprRes** outSprRes);

bool FindOpaqueBounds(const unsigned int* pixels, int width, int height, tagRECT* outBounds);
void UnpremultiplyPixels(std::vector<unsigned int>& pixels);

struct BillboardComposeConfig {
    int width = kPlayerBillboardComposeWidth;
    int height = kPlayerBillboardComposeHeight;
    int anchorX = kPlayerBillboardAnchorX;
    int anchorY = kPlayerBillboardAnchorY;
};

bool ResolveBillboardComposeConfig(const RECT& bounds, BillboardComposeConfig* outConfig)
{
    // Keep the actor's origin on the surface and leave transparent texels around
    // the artwork so texture filtering cannot sample a cropped edge.
    constexpr int padding = 2;
    const int left = (std::min)(static_cast<int>(bounds.left), 0) - padding;
    const int right = (std::max)(static_cast<int>(bounds.right), 1) + padding;
    const int top = (std::min)(static_cast<int>(bounds.top), 0) - padding;
    const int bottom = (std::max)(static_cast<int>(bounds.bottom), 1) + padding;
    if (!outConfig || right - left > 4096 || bottom - top > 4096) {
        return false;
    }
    outConfig->width = right - left;
    outConfig->height = bottom - top;
    outConfig->anchorX = -left;
    outConfig->anchorY = -top;
    return true;
}

bool ResolveNonPcBillboardMotion(const CPc& actor,
    int bodyAction,
    int motion,
    CActRes** outActRes,
    CSprRes** outSprRes,
    const CMotion** outMotion)
{
    if (!outActRes || !outSprRes || !outMotion) {
        return false;
    }

    *outActRes = nullptr;
    *outSprRes = nullptr;
    *outMotion = nullptr;

    CActRes* actRes = nullptr;
    CSprRes* sprRes = nullptr;
    if (!ResolveCachedNonPcResourcesForActor(const_cast<CPc&>(actor), &actRes, &sprRes) || !actRes || !sprRes) {
        return false;
    }

    int resolvedAction = bodyAction;
    if (actRes->GetMotionCount(resolvedAction) <= 0 && resolvedAction >= 8) {
        resolvedAction -= 8;
    }
    if (actRes->GetMotionCount(resolvedAction) <= 0) {
        resolvedAction = 0;
    }
    if (actRes->GetMotionCount(resolvedAction) <= 0) {
        return false;
    }

    const CMotion* spriteMotion = actRes->GetMotion(resolvedAction, motion);
    if (!spriteMotion) {
        spriteMotion = actRes->GetMotion(resolvedAction, 0);
    }
    if (!spriteMotion) {
        return false;
    }

    *outActRes = actRes;
    *outSprRes = sprRes;
    *outMotion = spriteMotion;
    return true;
}

bool ResolveNonPcBillboardComposeConfig(const CPc& actor,
    int bodyAction,
    int motion,
    BillboardComposeConfig* outConfig,
    const ActorBillboardAnimation& animation)
{
    if (!outConfig) {
        return false;
    }

    CActRes* actRes = nullptr;
    CSprRes* sprRes = nullptr;
    const CMotion* spriteMotion = nullptr;
    if (!ResolveNonPcBillboardMotion(actor, bodyAction, motion, &actRes, &sprRes, &spriteMotion)) {
        return false;
    }

    RECT clipBounds{};
    const CMotion* nextMotion = spriteMotion;
    if (animation.enabled) {
        CActRes* nextAct = nullptr;
        CSprRes* nextSpr = nullptr;
        ResolveNonPcBillboardMotion(actor, bodyAction, animation.body.next, &nextAct, &nextSpr, &nextMotion);
    }
    if (!ComputeBillboardMotionBounds(sprRes, spriteMotion, nextMotion,
            animation.body.Fraction(), animation, &clipBounds)) {
        return false;
    }

    return ResolveBillboardComposeConfig(clipBounds, outConfig);
}
bool DrawPcBillboard(BillboardComposeSurface& bitmap,
    const CPc& actor,
    int drawX,
    int drawY,
    int bodyAction,
    int headMotion,
    const std::array<int, 3>& accessoryMotions,
    int* outJob,
    int* outHead,
    int* outSex,
    int* outBodyPalette,
    int* outHeadPalette);

bool ResolvePlayerBillboardComposeConfig(const CPc& actor, int bodyAction, int headMotion,
    const std::array<int, 3>& accessoryMotions, const ActorBillboardAnimation& animation,
    BillboardComposeConfig* outConfig)
{
    BillboardComposeSurface boundsSurface(animation);
    RECT bounds{};
    if (!DrawPcBillboard(boundsSurface, actor, 0, 0, bodyAction, headMotion, accessoryMotions,
            nullptr, nullptr, nullptr, nullptr, nullptr)
        || !boundsSurface.GetMotionBounds(&bounds)) {
        return false;
    }
    return ResolveBillboardComposeConfig(bounds, outConfig);
}

std::array<int, 3> ResolvePcAccessoryMotionIndices(const CPc& actor,
    int bodyAction, int headMotion, u32 elapsedMs);

struct SharedPlayerBillboardStripKey {
    int job = 0;
    int sex = 0;
    int head = 0;
    int bodyPalette = 0;
    int bodyStyle = 0;
    int headPalette = 0;
    int accessoryBottom = 0;
    int accessoryMid = 0;
    int accessoryTop = 0;
    int weapon = 0;
    int shield = 0;
    int action = 0;

    bool operator<(const SharedPlayerBillboardStripKey& other) const
    {
        if (job != other.job) {
            return job < other.job;
        }
        if (sex != other.sex) {
            return sex < other.sex;
        }
        if (head != other.head) {
            return head < other.head;
        }
        if (bodyPalette != other.bodyPalette) {
            return bodyPalette < other.bodyPalette;
        }
        if (bodyStyle != other.bodyStyle) {
            return bodyStyle < other.bodyStyle;
        }
        if (headPalette != other.headPalette) {
            return headPalette < other.headPalette;
        }
        if (accessoryBottom != other.accessoryBottom) {
            return accessoryBottom < other.accessoryBottom;
        }
        if (accessoryMid != other.accessoryMid) {
            return accessoryMid < other.accessoryMid;
        }
        if (accessoryTop != other.accessoryTop) {
            return accessoryTop < other.accessoryTop;
        }
        if (weapon != other.weapon) {
            return weapon < other.weapon;
        }
        if (shield != other.shield) {
            return shield < other.shield;
        }
        return action < other.action;
    }
};

std::map<SharedPlayerBillboardKey, SharedPlayerBillboardValue>& GetSharedPlayerBillboardCache()
{
    static std::map<SharedPlayerBillboardKey, SharedPlayerBillboardValue> cache;
    return cache;
}

std::set<SharedPlayerBillboardStripKey>& GetPrimedPlayerBillboardStrips()
{
    static std::set<SharedPlayerBillboardStripKey> strips;
    return strips;
}

SharedPlayerBillboardKey BuildSharedPlayerBillboardKey(const CPc& actor, int displayJob, int sex)
{
    return SharedPlayerBillboardKey{
        displayJob,
        sex,
        actor.m_head,
        actor.m_bodyPalette,
        actor.m_bodyStyle,
        actor.m_headPalette,
        actor.m_accessory,
        actor.m_accessory3,
        actor.m_accessory2,
        actor.m_weapon,
        actor.m_shield,
        0,
        0,
    };
}

bool PopulateSharedPlayerBillboardFrame(CPc& actor,
    const SharedPlayerBillboardKey& sharedPlayerKey,
    int displayJob,
    int sex,
    int bodyAction,
    int motionIndex)
{
    SharedPlayerBillboardKey frameKey = sharedPlayerKey;
    frameKey.action = bodyAction;
    frameKey.motion = motionIndex;

    auto& sharedCache = GetSharedPlayerBillboardCache();
    const auto existing = sharedCache.find(frameKey);
    if (existing != sharedCache.end()) {
        existing->second.lastUsed = NextBillboardCacheUse();
        return true;
    }

    BillboardComposeConfig composeConfig{};
    if (!ResolvePlayerBillboardComposeConfig(actor, bodyAction, motionIndex,
            frameKey.accessoryMotions, frameKey.animation, &composeConfig)) {
        return false;
    }

    BillboardComposeSurface stripSurface(composeConfig.width, composeConfig.height,
        frameKey.animation.sharpPixels);
    stripSurface.animation = frameKey.animation;
    stripSurface.Clear(0x00000000u);

    int stripResolvedJob = -1;
    int stripResolvedHead = -1;
    int stripResolvedSex = -1;
    int stripResolvedBodyPalette = -1;
    int stripResolvedHeadPalette = -1;
    if (!DrawPcBillboard(stripSurface,
            actor,
            composeConfig.anchorX,
            composeConfig.anchorY,
            bodyAction,
            motionIndex,
            frameKey.accessoryMotions,
            &stripResolvedJob,
            &stripResolvedHead,
            &stripResolvedSex,
            &stripResolvedBodyPalette,
            &stripResolvedHeadPalette)) {
        return false;
    }

    tagRECT stripOpaqueBounds{};
    if (!FindOpaqueBounds(stripSurface.GetImageData(),
            static_cast<int>(stripSurface.GetWidth()),
            static_cast<int>(stripSurface.GetHeight()),
            &stripOpaqueBounds)) {
        return false;
    }
    stripSurface.ToLogicalBounds(&stripOpaqueBounds);

    std::vector<unsigned int> stripPixels(stripSurface.GetImageData(),
        stripSurface.GetImageData() + static_cast<size_t>(stripSurface.GetWidth()) * static_cast<size_t>(stripSurface.GetHeight()));
    UnpremultiplyPixels(stripPixels);

    CTexture* stripTexture = new CTexture();
    if (!stripTexture) {
        return false;
    }
    if (!stripTexture->Create(stripSurface.GetWidth(), stripSurface.GetHeight(), PF_A8R8G8B8, false)) {
        delete stripTexture;
        return false;
    }

    SetActorBillboardDebugName(stripTexture,
        actor,
        displayJob,
        bodyAction,
        motionIndex,
        true);
    stripTexture->Update(0,
        0,
        stripSurface.GetWidth(),
        stripSurface.GetHeight(),
        stripPixels.data(),
        true,
        stripSurface.GetWidth() * static_cast<int>(sizeof(unsigned int)));

    SharedPlayerBillboardValue stripValue{};
    stripValue.texture.reset(stripTexture);
    stripValue.lastUsed = NextBillboardCacheUse();
    stripValue.opaqueBounds = stripOpaqueBounds;
    stripValue.width = composeConfig.width;
    stripValue.height = composeConfig.height;
    stripValue.anchorX = composeConfig.anchorX;
    stripValue.anchorY = composeConfig.anchorY;
    sharedCache[frameKey] = stripValue;
    TrimSharedBillboardCache(sharedCache);
    return true;
}

void PrimePlayerBillboardStrip(CPc& actor,
    const SharedPlayerBillboardKey& sharedPlayerKey,
    int displayJob,
    int sex,
    int bodyAction)
{
    char bodyAct[260] = {};
    const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyAct, actor.m_bodyStyle);
    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    const int motionCount = bodyActRes ? bodyActRes->GetMotionCount(bodyAction) : 0;
    if (motionCount <= 1 || motionCount > kSharedPlayerWarmupMotionLimit) {
        return;
    }

    SharedPlayerBillboardStripKey stripKey{
        sharedPlayerKey.job,
        sharedPlayerKey.sex,
        sharedPlayerKey.head,
        sharedPlayerKey.bodyPalette,
        sharedPlayerKey.bodyStyle,
        sharedPlayerKey.headPalette,
        sharedPlayerKey.accessoryBottom,
        sharedPlayerKey.accessoryMid,
        sharedPlayerKey.accessoryTop,
        sharedPlayerKey.weapon,
        sharedPlayerKey.shield,
        bodyAction,
    };

    std::set<SharedPlayerBillboardStripKey>& primedStrips = GetPrimedPlayerBillboardStrips();
    const bool inserted = primedStrips.insert(stripKey).second;
    if (!inserted) {
        SharedPlayerBillboardKey firstFrameKey = sharedPlayerKey;
        firstFrameKey.action = bodyAction;
        firstFrameKey.motion = 0;
        firstFrameKey.accessoryMotions = ResolvePcAccessoryMotionIndices(actor, bodyAction, 0, 0);
        firstFrameKey.animation.enabled = GetCachedGraphicsSettings().spriteInterpolation;
        firstFrameKey.animation.sharpPixels = GetCachedGraphicsSettings().sharpSprites;
        firstFrameKey.animation.sharpness = firstFrameKey.animation.sharpPixels
            ? GetCachedGraphicsSettings().spriteSharpness : 4;
        firstFrameKey.animation.body = ActMotionSample::Pose(0);
        for (size_t slot = 0; slot < firstFrameKey.accessoryMotions.size(); ++slot) {
            firstFrameKey.animation.accessories[slot] = ActMotionSample::Pose(firstFrameKey.accessoryMotions[slot]);
        }
        if (GetSharedPlayerBillboardCache().find(firstFrameKey) != GetSharedPlayerBillboardCache().end()) {
            return;
        }
    }

    for (int motionIndex = 0; motionIndex < motionCount; ++motionIndex) {
        SharedPlayerBillboardKey frameKey = sharedPlayerKey;
        frameKey.accessoryMotions = ResolvePcAccessoryMotionIndices(actor, bodyAction, motionIndex, 0);
        frameKey.animation.enabled = GetCachedGraphicsSettings().spriteInterpolation;
        frameKey.animation.sharpPixels = GetCachedGraphicsSettings().sharpSprites;
        frameKey.animation.sharpness = frameKey.animation.sharpPixels
            ? GetCachedGraphicsSettings().spriteSharpness : 4;
        frameKey.animation.body = ActMotionSample::Pose(motionIndex);
        for (size_t slot = 0; slot < frameKey.accessoryMotions.size(); ++slot) {
            frameKey.animation.accessories[slot] = ActMotionSample::Pose(frameKey.accessoryMotions[slot]);
        }
        PopulateSharedPlayerBillboardFrame(actor, frameKey, displayJob, sex, bodyAction, motionIndex);
    }
}

struct SharedItemBillboardKey {
    u32 itemId = 0;
    u8 identified = 0;
    int motion = 0;

    bool operator<(const SharedItemBillboardKey& other) const
    {
        if (itemId != other.itemId) {
            return itemId < other.itemId;
        }
        if (identified != other.identified) {
            return identified < other.identified;
        }
        return motion < other.motion;
    }
};

struct SharedItemBillboardValue {
    CTexture* texture = nullptr;
    tagRECT opaqueBounds{};
    int width = 0;
    int height = 0;
    int anchorX = kItemBillboardAnchorX;
    int anchorY = kItemBillboardAnchorY;
};

std::map<SharedItemBillboardKey, SharedItemBillboardValue>& GetSharedItemBillboardCache()
{
    static std::map<SharedItemBillboardKey, SharedItemBillboardValue> cache;
    return cache;
}

void ReleaseActorBillboardTexture(CPc& actor)
{
    if (actor.m_billboardTextureOwned && actor.m_billboardTexture) {
        delete actor.m_billboardTexture;
    }
    actor.m_billboardTexture = nullptr;
    actor.m_sharedBillboardTexture.reset();
    actor.m_billboardTextureOwned = 0;
}

} // namespace

void SetRuntimeActorCameraLongitude(float longitude)
{
    g_runtimeActorCameraLongitude = longitude;
}

void ClearSharedActorBillboardCaches()
{
    GetSharedPlayerBillboardCache().clear();
    GetSharedNonPcBillboardCache().clear();
    GetPrimedPlayerBillboardStrips().clear();
    ClearSpriteInterpolationCache();
}

int CGameObject::Get8Dir(float rot)
{
    return ResolveEightDirectionFromLongitude(ResolveRuntimeActorLongitudeForDirection(rot), false);
}

int CGameActor::Get8Dir(float rot)
{
    return ResolveEightDirectionFromLongitude(
        ResolveRuntimeActorLongitudeForDirection(rot),
        ShouldUseRoundedDirectionForActor(this));
}

namespace {

float ResolveActorMotionProgress(const CRenderObject& object, CActRes* actRes, int action)
{
    const CGameActor* actor = dynamic_cast<const CGameActor*>(&object);
    const float actionDelay = actRes ? actRes->GetDelay(action) : kDefaultMotionDelay;
    if (actor && actor->m_isMoving && (action & ~7) == 8) {
        // Keep the walk cycle tied to distance, including across path updates.
        return (std::max)(0.0f, actor->m_dist) * 1.48f / actionDelay;
    }

    const float delay = object.m_motionType == 1 ? object.m_motionSpeed : actionDelay;
    const float safeDelay = std::isfinite(delay) && delay > 0.0f
        ? (std::max)(kMinimumMotionDelay, delay)
        : kDefaultMotionDelay;
    const u32 elapsedMs = timeGetTime() - object.m_stateStartTick;
    return static_cast<float>(elapsedMs) / (safeDelay * kActDelayUnitMs);
}

int ResolveMotionFrame(float progress, int motionCount, bool holdLastFrame)
{
    if (motionCount <= 1 || !std::isfinite(progress) || progress <= 0.0f) {
        return 0;
    }
    if (holdLastFrame && progress >= static_cast<float>(motionCount)) {
        return motionCount - 1;
    }
    // Reduce before converting to int, even for long-lived looping actors.
    return static_cast<int>(std::fmod(progress, static_cast<float>(motionCount)));
}

ActMotionSample ResolveActorRenderSample(const CGameActor& actor, CActRes* actRes,
    int action, int selectedMotion)
{
    if (!GetCachedGraphicsSettings().spriteInterpolation || !actRes
        || actor.IsStatusAnimationFrozen() || actor.m_isMotionFreezed
        || actor.m_isMotionFinished || actor.m_isNeverAnimation
        || (actor.m_isPc && ((action & ~7) == 0 || (action & ~7) == 16))) {
        return ActMotionSample::Pose(selectedMotion);
    }
    const float delay = actor.m_motionType == 1 ? actor.m_motionSpeed : actRes->GetDelay(action);
    double durationMs = delay * kActDelayUnitMs;
    if (actor.m_isMoving && (action & ~7) == 8) {
        // Progress is distance based. Derive the time per frame from the active
        // path segment without changing its distance clock or walking cadence.
        size_t segment = 0;
        const CWorld* world = &g_world;
        if (world && FindActivePathSegment(actor.m_path, g_session.GetServerTime(), &segment)
            && segment + 1 < actor.m_path.m_cells.size()) {
            const auto& start = actor.m_path.m_cells[segment];
            const auto& end = actor.m_path.m_cells[segment + 1];
            const float startX = segment == 0 ? actor.m_moveStartPos.x : TileToWorldCoordX(world, start.x);
            const float startZ = segment == 0 ? actor.m_moveStartPos.z : TileToWorldCoordZ(world, start.y);
            const float dx = TileToWorldCoordX(world, end.x) - startX;
            const float dz = TileToWorldCoordZ(world, end.y) - startZ;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance > 0.001f) {
                durationMs = static_cast<double>(end.arrivalTime - start.arrivalTime)
                    * actRes->GetDelay(action) / (distance * 1.48f);
            }
        }
    }
    const auto sample = SampleActMotion(ResolveActorMotionProgress(actor, actRes, action),
        actRes->GetMotionCount(action), durationMs, actor.m_motionType == 1);
    // State processing owns the discrete pose and its events. A clock boundary
    // crossed while composing must not combine unrelated frames.
    return sample.current == selectedMotion ? sample : ActMotionSample::Pose(selectedMotion);
}

int ResolvePcMotionIndex(const CPc* actor, int action, const std::string& bodyActName)
{
    if (!actor || !actor->m_isMoving) {
        return 0;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return 0;
    }

    const int motionCount = bodyActRes->GetMotionCount(action);
    if (motionCount <= 1) {
        return 0;
    }

    return ResolveMotionFrame(ResolveActorMotionProgress(*actor, bodyActRes, action), motionCount, false);
}

int ResolveAvailableActionIndex(CActRes* actRes, int action)
{
    if (!actRes) {
        return -1;
    }

    if (actRes->GetMotionCount(action) > 0) {
        return action;
    }

    const int familyBase = action & ~7;
    if (familyBase != action && familyBase >= 0 && actRes->GetMotionCount(familyBase) > 0) {
        return familyBase;
    }

    for (int candidate = familyBase - 8; candidate >= 0; candidate -= 8) {
        if (actRes->GetMotionCount(candidate) > 0) {
            return candidate;
        }
    }

    return -1;
}

int ResolveTimedMotionIndex(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes) {
        return 0;
    }

    const int resolvedAction = ResolveAvailableActionIndex(actRes, action);
    if (resolvedAction < 0) {
        return 0;
    }

    const int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 1) {
        return 0;
    }

    if (actor.IsStatusAnimationFrozen() || actor.m_isMotionFreezed
        || (actor.m_isMotionFinished && actor.m_motionType == 1)) {
        return (std::max)(0, (std::min)(actor.m_curMotion, motionCount - 1));
    }
    return ResolveMotionFrame(ResolveActorMotionProgress(actor, actRes, resolvedAction),
        motionCount, actor.m_motionType == 1);
}

bool IsTransientActionActive(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes || action <= 0) {
        return false;
    }

    const int actionBand = action & ~7;
    if (actor.m_stateId == 0 && (actionBand == 0 || actionBand == 8 || actionBand == 16)) {
        return false;
    }
    if (actor.m_stateStartTick == 0) {
        return false;
    }

    const bool allowWhileMoving = actor.m_stateId == kAttackStateId
        || actor.m_stateId == kSecondAttackStateId
        || actor.m_stateId == kGameActorPickupStateId
        || actor.m_stateId == kDeathStateId;
    if (actor.m_isMoving && !allowWhileMoving) {
        return false;
    }

    const int resolvedAction = ResolveAvailableActionIndex(actRes, action);
    if (resolvedAction < 0) {
        return false;
    }

    if (actor.m_stateId == kDeathStateId) {
        return actor.m_vanishTime == 0 || timeGetTime() < actor.m_vanishTime;
    }
    if (actor.m_stateId != 0) {
        // State processing owns completion, including freezes and recovery.
        return true;
    }
    if (actor.m_isPc != 0 && actionBand == 32) {
        // The player's ready stance loops until a state change replaces it.
        return true;
    }
    // Other action bands retained in idle finish after one cycle.
    const int motionCount = (std::max)(1, actRes->GetMotionCount(resolvedAction));
    return ResolveActorMotionProgress(actor, actRes, resolvedAction) < static_cast<float>(motionCount);
}

int ResolveSpriteMotionIndex(const CGameActor& actor, CActRes* actRes, int action)
{
    if (!actRes) {
        return 0;
    }

    int resolvedAction = action;
    int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 0 && resolvedAction >= 8) {
        resolvedAction -= 8;
        motionCount = actRes->GetMotionCount(resolvedAction);
    }
    if (motionCount <= 1) {
        return 0;
    }

    if (!actor.m_isMoving) {
        return ResolveTimedMotionIndex(actor, actRes, resolvedAction);
    }

    return ResolveMotionFrame(ResolveActorMotionProgress(actor, actRes, resolvedAction), motionCount, false);
}

int ResolvePcBodyActionFromView(float cameraLongitude, float actorRotationDegrees)
{
    const float longitude = NormalizeAngle360(-cameraLongitude - actorRotationDegrees);
    if (longitude < 22.5f || longitude > 337.5f) {
        return 0;
    }
    return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
}

float NormalizeSignedAngle180(float angle)
{
    angle = std::fmod(angle, 360.0f);
    if (angle > 180.0f) {
        angle -= 360.0f;
    } else if (angle < -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

int ResolvePcBodyActionFromViewStable(float cameraLongitude, float actorRotationDegrees, int previousDir)
{
    const float longitude = NormalizeAngle360(-cameraLongitude - actorRotationDegrees);
    const int resolvedDir = ResolvePcBodyActionFromView(cameraLongitude, actorRotationDegrees);
    if (previousDir < 0 || previousDir > 7) {
        return resolvedDir;
    }

    constexpr float kDirectionHysteresisDeg = 6.0f;
    const float previousCenter = static_cast<float>(previousDir) * 45.0f;
    const float deltaToPrevious = std::fabs(NormalizeSignedAngle180(longitude - previousCenter));
    if (deltaToPrevious <= 22.5f + kDirectionHysteresisDeg) {
        return previousDir;
    }
    return resolvedDir;
}

int ResolveActionDirFromView(float cameraLongitude, float actorRotationDegrees, bool useRoundedPlayerDirs)
{
    const float longitude = NormalizeAngle360(-cameraLongitude - actorRotationDegrees);
    if (useRoundedPlayerDirs) {
        if (longitude < 22.5f || longitude > 337.5f) {
            return 0;
        }
        return static_cast<int>((longitude + 22.5f) / 45.0f) & 7;
    }

    return static_cast<int>(longitude / 45.0f) & 7;
}

int ResolveDirectionalActionFromView(CActRes* actRes,
    const CGameActor& actor,
    float cameraLongitude,
    bool isPlayerStyleActor)
{
    const bool useRoundedPlayerDirs = isPlayerStyleActor
        && (actor.m_baseAction == 0 || actor.m_baseAction == 8 || actor.m_baseAction == 16);
    const int directionalAction = actor.m_baseAction
        + ResolveActionDirFromView(cameraLongitude, actor.m_roty, useRoundedPlayerDirs);

    const int resolvedDirectionalAction = ResolveAvailableActionIndex(actRes, directionalAction);
    if (resolvedDirectionalAction >= 0) {
        return resolvedDirectionalAction;
    }

    const int resolvedBaseAction = ResolveAvailableActionIndex(actRes, actor.m_baseAction);
    if (resolvedBaseAction >= 0) {
        return resolvedBaseAction;
    }

    return directionalAction;
}

int ResolveTransientBillboardAction(CActRes* actRes,
    const CGameActor& actor,
    float cameraLongitude,
    bool isPlayerStyleActor)
{
    if (!actRes) {
        return actor.m_baseAction;
    }

    return ResolveDirectionalActionFromView(actRes, actor, cameraLongitude, isPlayerStyleActor);
}

void ResolveTransientSpriteState(const CGameActor& actor,
    CActRes* actRes,
    float cameraLongitude,
    bool isPlayerStyleActor,
    int* outAction,
    int* outMotion)
{
    if (!outAction || !outMotion) {
        return;
    }

    if (!actRes) {
        *outAction = actor.m_baseAction;
        *outMotion = 0;
        return;
    }

    const int resolvedAction = ResolveTransientBillboardAction(actRes, actor, cameraLongitude, isPlayerStyleActor);
    *outAction = resolvedAction;

    const int motionCount = actRes->GetMotionCount(resolvedAction);
    if (motionCount <= 0) {
        *outMotion = 0;
        return;
    }

    if (actor.IsStatusAnimationFrozen() || actor.m_isMotionFinished || actor.m_isMotionFreezed) {
        *outMotion = (std::min)(actor.m_curMotion, motionCount - 1);
        return;
    }

    *outMotion = ResolveMotionFrame(ResolveActorMotionProgress(actor, actRes, resolvedAction),
        motionCount, actor.m_motionType == 1);
}

void LogDeathBillboardSelectionOnce(const CGameActor& actor,
    CActRes* actRes,
    int resolvedAction,
    int resolvedMotion)
{
    if (actor.m_stateId != kDeathStateId || !actRes || actor.m_gid == 0) {
        return;
    }

    static std::map<u32, u32> loggedDeathStateTicks;
    const auto it = loggedDeathStateTicks.find(actor.m_gid);
    if (it != loggedDeathStateTicks.end() && it->second == actor.m_stateStartTick) {
        return;
    }

    loggedDeathStateTicks[actor.m_gid] = actor.m_stateStartTick;
    DbgLog("[Actor] death billboard gid=%u job=%d name='%s' baseAction=%d curAction=%d resolvedAction=%d resolvedMotion=%d motionCount=%d vanish=%u roty=%.2f dir=%d\n",
        actor.m_gid,
        actor.m_job,
        g_session.GetJobName(actor.m_job),
        actor.m_baseAction,
        actor.m_curAction,
        resolvedAction,
        resolvedMotion,
        actRes->GetMotionCount(resolvedAction),
        actor.m_vanishTime,
        actor.m_roty,
        const_cast<CGameActor&>(actor).Get8Dir(actor.m_roty));

    if (actor.m_job == 1002) {
        for (int action = 64; action <= 71; ++action) {
            const int motionCount = actRes->GetMotionCount(action);
            int firstSpr = -1;
            int secondSpr = -1;
            if (motionCount > 0) {
                if (const CMotion* motion0 = actRes->GetMotion(action, 0)) {
                    if (!motion0->sprClips.empty()) {
                        firstSpr = motion0->sprClips[0].sprIndex;
                    }
                }
            }
            if (motionCount > 1) {
                if (const CMotion* motion1 = actRes->GetMotion(action, 1)) {
                    if (!motion1->sprClips.empty()) {
                        secondSpr = motion1->sprClips[0].sprIndex;
                    }
                }
            }
            DbgLog("[Actor] death family job=%d action=%d motions=%d spr0=%d spr1=%d\n",
                actor.m_job,
                action,
                motionCount,
                firstSpr,
                secondSpr);
        }
    }
}

void LogDeathMotionProgress(const CGameActor& actor)
{
    if (actor.m_stateId != kDeathStateId || actor.m_gid == 0) {
        return;
    }

    struct DeathProgressState {
        u32 stateTick = 0;
        int lastAction = -1;
        int lastMotion = -1;
    };

    static std::map<u32, DeathProgressState> progressByGid;
    DeathProgressState& state = progressByGid[actor.m_gid];
    if (state.stateTick != actor.m_stateStartTick) {
        state.stateTick = actor.m_stateStartTick;
        state.lastAction = -1;
        state.lastMotion = -1;
    }

    if (state.lastAction == actor.m_curAction && state.lastMotion == actor.m_curMotion) {
        return;
    }

    state.lastAction = actor.m_curAction;
    state.lastMotion = actor.m_curMotion;
    DbgLog("[Actor] death progress gid=%u action=%d motion=%d roty=%.2f vanish=%u elapsed=%u\n",
        actor.m_gid,
        actor.m_curAction,
        actor.m_curMotion,
        actor.m_roty,
        actor.m_vanishTime,
        actor.m_stateStartTick != 0 ? (timeGetTime() - actor.m_stateStartTick) : 0);
}

int ResolveHeadMotionFromBodyAction(int bodyAction, int headDir)
{
    const int normalizedHeadDir = (std::max)(0, (std::min)(headDir, 2));
    const int bodyBand = bodyAction & ~7;
    if (bodyBand == 0 || bodyBand == 16) {
        return normalizedHeadDir;
    }
    return -1;
}

std::array<int, 3> ResolvePcAccessoryMotionIndices(const CPc& actor,
    int bodyAction, int headMotion, u32 elapsedMs)
{
    std::array<int, 3> motions{};
    const std::array<int, 3> accessories{ actor.m_accessory, actor.m_accessory3, actor.m_accessory2 };
    const int displayJob = ResolveDisplayJob(actor);
    const int sex = actor.m_sex != 0 ? 1 : 0;
    for (size_t slot = 0; slot < accessories.size(); ++slot) {
        if (accessories[slot] <= 0) {
            continue;
        }
        char actPath[260] = {};
        int head = actor.m_head;
        g_session.GetAccessoryActName(displayJob, &head, sex, accessories[slot], actPath);
        CActRes* actRes = actPath[0] ? g_resMgr.GetAs<CActRes>(actPath) : nullptr;
        motions[slot] = actRes ? actRes->GetAccessoryMotionIndex(bodyAction, headMotion, elapsedMs) : 0;
    }
    return motions;
}

std::array<ActMotionSample, 3> ResolvePcAccessoryMotionSamples(const CPc& actor,
    int bodyAction, const ActMotionSample& body, u32 elapsedMs)
{
    std::array<ActMotionSample, 3> samples{};
    const std::array<int, 3> accessories{ actor.m_accessory, actor.m_accessory3, actor.m_accessory2 };
    const int displayJob = ResolveDisplayJob(actor);
    const int sex = actor.m_sex != 0 ? 1 : 0;
    for (size_t slot = 0; slot < accessories.size(); ++slot) {
        if (accessories[slot] <= 0) continue;
        char actPath[260] = {};
        int head = actor.m_head;
        g_session.GetAccessoryActName(displayJob, &head, sex, accessories[slot], actPath);
        CActRes* actRes = actPath[0] ? g_resMgr.GetAs<CActRes>(actPath) : nullptr;
        if (!actRes) continue;
        const int band = bodyAction & ~7;
        if (band == 0 || band == 16) {
            samples[slot] = actRes->GetAccessoryMotionSample(bodyAction, body.current, elapsedMs);
        } else {
            // Walking and combat equipment retain the character's pose clock.
            samples[slot] = body;
            samples[slot].current = actRes->GetAccessoryMotionIndex(bodyAction, body.current, elapsedMs);
            samples[slot].next = actRes->GetAccessoryMotionIndex(bodyAction, body.next, elapsedMs);
        }
        if (!GetCachedGraphicsSettings().spriteInterpolation) {
            samples[slot] = ActMotionSample::Pose(samples[slot].current);
        }
    }
    return samples;
}

bool BuildPaletteOverride(const std::string& paletteName, std::array<unsigned int, 256>& outPalette)
{
    if (paletteName.empty()) {
        return false;
    }

    CPaletteRes* palRes = g_resMgr.GetAs<CPaletteRes>(paletteName.c_str());
    if (!palRes) {
        return false;
    }

    g_3dDevice.ConvertPalette(outPalette.data(), palRes->m_pal, 256);
    return true;
}

u32 HashPixelBuffer(const unsigned int* pixels, size_t pixelCount)
{
    if (!pixels || pixelCount == 0) {
        return 0u;
    }

    u32 hash = 2166136261u;
    for (size_t index = 0; index < pixelCount; ++index) {
        hash ^= pixels[index];
        hash *= 16777619u;
    }
    return hash;
}

int ResolveClassicTransBaseJobForComparison(int job)
{
    if (job >= 4008 && job <= 4022) {
        return job - 4001;
    }

    return job;
}

bool ApplyAttachPointDelta(const CMotion* baseMotion, const CMotion* attachedMotion, POINT* inOutPoint)
{
    if (!baseMotion || !attachedMotion || !inOutPoint || baseMotion->attachInfo.empty() || attachedMotion->attachInfo.empty()) {
        return false;
    }

    const CAttachPointInfo& attached = attachedMotion->attachInfo.front();
    const CAttachPointInfo& base = baseMotion->attachInfo.front();
    if (attached.attr != base.attr) {
        return false;
    }

    inOutPoint->x += base.x - attached.x;
    inOutPoint->y += base.y - attached.y;
    return true;
}

POINT GetPlayerLayerPoint(int layerPriority,
    int resolvedLayer,
    CImfRes* imfRes,
    const CMotion* motion,
    const std::string& bodyActName,
    const std::string& headActName,
    int curAction,
    int bodyMotionIndex,
    int curMotion,
    int headMotionIndex)
{
    POINT point = imfRes->GetPoint(resolvedLayer, curAction, curMotion);
    if (!motion || motion->attachInfo.empty()) {
        return point;
    }

    if (layerPriority == 1) {
        CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
        if (!bodyActRes) {
            return point;
        }

        const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
        if (!bodyMotion || bodyMotion->attachInfo.empty()) {
            return point;
        }

        ApplyAttachPointDelta(bodyMotion, motion, &point);
        return point;
    }

    if (layerPriority != 2 && layerPriority != 3 && layerPriority != 4 && layerPriority != 8) {
        return point;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    if (!bodyActRes) {
        return point;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    if (!bodyMotion || bodyMotion->attachInfo.empty()) {
        return point;
    }

    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    if (!headActRes) {
        return point;
    }

    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    if (!headMotion) {
        return point;
    }

    POINT headOffset{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &headOffset);
    point.x += headOffset.x;
    point.y += headOffset.y;
    ApplyAttachPointDelta(headMotion, motion, &point);
    return point;
}

bool DrawAttachedAccessoryMotion(BillboardComposeSurface& bitmap,
    int drawX,
    int drawY,
    int curAction,
    int bodyMotionIndex,
    int headMotionIndex,
    int accessoryMotionIndex,
    const std::string& bodyActName,
    const std::string& headActName,
    const std::string& accessoryActName,
    const std::string& accessorySprName,
    size_t slot)
{
    if (accessoryActName.empty() || accessorySprName.empty()) {
        return false;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    CActRes* headActRes = g_resMgr.GetAs<CActRes>(headActName.c_str());
    CActRes* accessoryActRes = g_resMgr.GetAs<CActRes>(accessoryActName.c_str());
    CSprRes* accessorySprRes = g_resMgr.GetAs<CSprRes>(accessorySprName.c_str());
    if (!bodyActRes || !headActRes || !accessoryActRes || !accessorySprRes) {
        return false;
    }

    const CMotion* bodyMotion = bodyActRes->GetMotion(curAction, bodyMotionIndex);
    const CMotion* headMotion = headActRes->GetMotion(curAction, headMotionIndex);
    const CMotion* accessoryMotion = accessoryActRes->GetMotion(curAction, accessoryMotionIndex);
    if (!accessoryMotion) {
        accessoryMotion = accessoryActRes->GetMotion(curAction, 0);
    }
    if (!bodyMotion || !headMotion || !accessoryMotion) {
        return false;
    }

    POINT point{};
    ApplyAttachPointDelta(bodyMotion, headMotion, &point);
    ApplyAttachPointDelta(headMotion, accessoryMotion, &point);
    const auto& bodySample = bitmap.animation.body;
    const auto& sample = bitmap.animation.accessories[slot];
    const CMotion* nextAccessory = accessoryActRes->GetMotion(curAction, sample.next);
    const CMotion* nextBody = bodyActRes->GetMotion(curAction, bodySample.next);
    const CMotion* nextHead = headActRes->GetMotion(curAction, bodySample.next);
    POINT bodyPoint = point;
    if (nextBody && nextHead) {
        bodyPoint = {};
        ApplyAttachPointDelta(nextBody, nextHead, &bodyPoint);
        ApplyAttachPointDelta(nextHead, accessoryMotion, &bodyPoint);
    }
    POINT accessoryPoint = point;
    if (nextAccessory) {
        accessoryPoint = {};
        ApplyAttachPointDelta(bodyMotion, headMotion, &accessoryPoint);
        ApplyAttachPointDelta(headMotion, nextAccessory, &accessoryPoint);
    }
    const float x = static_cast<float>(drawX + point.x)
        + (bodyPoint.x - point.x) * bodySample.Fraction()
        + (accessoryPoint.x - point.x) * sample.Fraction();
    const float y = static_cast<float>(drawY + point.y)
        + (bodyPoint.y - point.y) * bodySample.Fraction()
        + (accessoryPoint.y - point.y) * sample.Fraction();
    bitmap.BltInterpolatedSprite(x, y, accessorySprRes, accessoryMotion,
        nextAccessory, accessorySprRes->m_pal, sample.Fraction());
    return true;
}

constexpr const char* kHumanSpriteBodyDirMarker = "\\\xB8\xF6\xC5\xEB\\";
constexpr const char* kShieldSpriteRoot = "data\\sprite\\\xB9\xE6\xC6\xD0\\";

constexpr const char* kWeaponTokenDagger = "\xB4\xDC\xB0\xCB";
constexpr const char* kWeaponTokenSword = "\xB0\xCB";
constexpr const char* kWeaponTokenAxe = "\xB5\xB5\xB3\xA2";
constexpr const char* kWeaponTokenSpear = "\xC3\xA2";
constexpr const char* kWeaponTokenClub = "\xC5\xAC\xB7\xB4";
constexpr const char* kWeaponTokenRod = "\xB7\xCE\xB5\xE5";
constexpr const char* kWeaponTokenBow = "\xC8\xB0";
constexpr const char* kWeaponTokenBook = "\xC3\xA5";
constexpr const char* kWeaponTokenKnuckle = "\xB3\xCA\xC5\xAC";
constexpr const char* kWeaponTokenInstrument = "\xBE\xC7\xB1\xE2";
constexpr const char* kWeaponTokenWhip = "\xC3\xA4\xC2\xEF";
constexpr const char* kWeaponTokenKatar = "\xC4\xAB\xC5\xB8\xB8\xA3";
constexpr const char* kWeaponTokenPistol = "\xB1\xC7\xC3\xD1";
constexpr const char* kWeaponTokenRifle = "\xB6\xF3\xC0\xCC\xC7\xC3";
constexpr const char* kWeaponTokenGatling = "\xB1\xE2\xB0\xFC\xC3\xD1";
constexpr const char* kWeaponTokenShotgun = "\xBC\xA6\xB0\xC7";
constexpr const char* kWeaponTokenShuriken = "\xBC\xF6\xB8\xAE\xB0\xCB";
constexpr const char* kWeaponTokenLightSuffix = "\xB0\xCB\xB1\xA4";

constexpr const char* kShieldTokenGuard = "guard";
constexpr const char* kShieldTokenBuckler = "buckler";
constexpr const char* kShieldTokenShield = "shield";
constexpr const char* kShieldTokenMirrorShield = "mirrorshield";

const char* GetGenericWeaponToken(int weaponType)
{
    switch (weaponType) {
    case 1:
        return kWeaponTokenDagger;
    case 2:
    case 3:
        return kWeaponTokenSword;
    case 4:
    case 5:
        return kWeaponTokenSpear;
    case 6:
    case 7:
        return kWeaponTokenAxe;
    case 8:
    case 9:
        return kWeaponTokenClub;
    case 10:
    case 23:
        return kWeaponTokenRod;
    case 11:
        return kWeaponTokenBow;
    case 12:
        return kWeaponTokenKnuckle;
    case 13:
        return kWeaponTokenInstrument;
    case 14:
        return kWeaponTokenWhip;
    case 15:
        return kWeaponTokenBook;
    case 16:
        return nullptr;
    case 17:
        return kWeaponTokenPistol;
    case 18:
        return kWeaponTokenRifle;
    case 19:
        return kWeaponTokenGatling;
    case 20:
        return kWeaponTokenShotgun;
    case 22:
        return kWeaponTokenShuriken;
    default:
        return nullptr;
    }
}

std::string GetPcOverlayWeaponToken(int weaponType)
{
    switch (weaponType) {
    case 16:
        return std::string(kWeaponTokenKatar) + "_" + kWeaponTokenKatar;
    case 25:
        return std::string(kWeaponTokenDagger) + "_" + kWeaponTokenDagger;
    case 26:
        return std::string(kWeaponTokenSword) + "_" + kWeaponTokenSword;
    case 27:
        return std::string(kWeaponTokenAxe) + "_" + kWeaponTokenAxe;
    case 28:
        return std::string(kWeaponTokenDagger) + "_" + kWeaponTokenSword;
    case 29:
        return std::string(kWeaponTokenDagger) + "_" + kWeaponTokenAxe;
    case 30:
        return std::string(kWeaponTokenSword) + "_" + kWeaponTokenAxe;
    default:
        if (const char* token = GetGenericWeaponToken(weaponType)) {
            return token;
        }
        return std::string();
    }
}

int NormalizeShieldViewId(int shield)
{
    if (shield <= 0) {
        return 0;
    }

    if (shield <= 4) {
        return shield;
    }

    switch (shield) {
    case 2101:
    case 2102:
        return 1;
    case 2103:
    case 2104:
        return 2;
    case 2105:
    case 2106:
        return 3;
    case 2107:
    case 2108:
    case 2110:
    case 2111:
        return 4;
    default:
        return 0;
    }
}

const char* GetShieldToken(int shieldViewId)
{
    switch (shieldViewId) {
    case 1:
        return kShieldTokenGuard;
    case 2:
        return kShieldTokenBuckler;
    case 3:
        return kShieldTokenShield;
    case 4:
        return kShieldTokenMirrorShield;
    default:
        return nullptr;
    }
}

bool ExtractPcOverlayPathParts(const std::string& bodyActName,
    std::string* outHumanOverlayRoot,
    std::string* outJobStem,
    std::string* outSexToken)
{
    if (!outHumanOverlayRoot || !outJobStem || !outSexToken) {
        return false;
    }

    const size_t bodyMarker = bodyActName.find(kHumanSpriteBodyDirMarker);
    if (bodyMarker == std::string::npos) {
        return false;
    }

    const size_t fileNameStart = bodyActName.find_last_of("\\/");
    if (fileNameStart == std::string::npos || fileNameStart + 1 >= bodyActName.size()) {
        return false;
    }

    const std::string fileName = bodyActName.substr(fileNameStart + 1);
    const size_t extension = fileName.rfind('.');
    if (extension == std::string::npos) {
        return false;
    }

    const std::string stem = fileName.substr(0, extension);
    const size_t lastSplit = stem.rfind('_');
    if (lastSplit == std::string::npos || lastSplit + 1 >= stem.size()) {
        return false;
    }

    size_t sexSplit = lastSplit;
    size_t sexLength = stem.size() - lastSplit - 1;
    const size_t secondSplit = lastSplit > 0 ? stem.rfind('_', lastSplit - 1) : std::string::npos;
    const auto isSexToken = [](const std::string& token) {
        return token == "\xB3\xB2" || token == "\xBF\xA9";
    };
    if (!isSexToken(stem.substr(lastSplit + 1))
        && secondSplit != std::string::npos && secondSplit + 1 < lastSplit
        && isSexToken(stem.substr(secondSplit + 1, lastSplit - secondSplit - 1))) {
        sexSplit = secondSplit;
        sexLength = lastSplit - secondSplit - 1;
    }

    if (sexSplit == std::string::npos || sexSplit + 1 >= stem.size()) {
        return false;
    }

    if (sexLength == 0) {
        return false;
    }

    *outJobStem = stem.substr(0, sexSplit);
    *outSexToken = stem.substr(sexSplit + 1, sexLength);
    *outHumanOverlayRoot = bodyActName.substr(0, bodyMarker) + "\\" + *outJobStem + "\\";
    return true;
}

int NormalizePcWeaponOverlayTypeForJob(int job, int weaponType, bool dualWeapon)
{
    if (weaponType <= 0) {
        if (dualWeapon && IsDualWeaponPcJob(job)) {
            return 2;
        }
        return 0;
    }

    if (dualWeapon) {
        if (IsDualWeaponPcJob(job) && (weaponType == 7 || weaponType == 8)) {
            return 2;
        }
        return weaponType;
    }

    if (((job == 6 || job == 4007 || job == 4029) && (weaponType == 6 || weaponType == 7 || weaponType == 8))
        || ((job == 12 || job == 17) && (weaponType == 7 || weaponType == 8))
        || ((job == 4013 || job == 4018) && (weaponType == 7 || weaponType == 8))
        || ((job == 4035 || job == 4040) && (weaponType == 7 || weaponType == 8))) {
        weaponType = 2;
    }

    if (((job == 2 || job == 9 || job == 16)
            || (job == 4003 || job == 4010 || job == 4017)
            || (job == 4025 || job == 4032 || job == 4039))
        && weaponType == 5) {
        weaponType = 10;
    }

    if ((job == 17 || job == 4018 || job == 4040) && weaponType == 6) {
        weaponType = 1;
    }

    if ((job == 8 || job == 4009 || job == 4031) && weaponType == 12) {
        return 0;
    }

    return weaponType;
}

bool SupportsPcWeaponOverlaySecondLayer(int weaponType)
{
    switch (weaponType) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 16:
    case 17:
    case 18:
    case 25:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
        return true;
    default:
        return false;
    }
}

int ResolvePcWeaponOverlayType(const CGameActor& actor, int weapon, bool* outDualWeapon)
{
    const int primaryWeaponItemId = weapon & 0xFFFF;
    const int secondaryWeaponItemId = static_cast<int>((static_cast<unsigned int>(weapon) >> 16) & 0xFFFFu);
    const int secondaryWeaponType = g_session.ResolvePackedWeaponType(actor.m_job, secondaryWeaponItemId);
    const bool dualWeapon = IsDualWeaponPcJob(actor.m_job) && secondaryWeaponType > 0;

    if (outDualWeapon) {
        *outDualWeapon = dualWeapon;
    }

    const int weaponType = g_session.ResolvePackedWeaponType(actor.m_job, weapon);

    return NormalizePcWeaponOverlayTypeForJob(actor.m_job, weaponType, dualWeapon);
}

bool TryBuildWeaponOverlayPath(const CGameActor& actor,
    const std::string& bodyActName,
    int weapon,
    bool secondLayer,
    const char* extension,
    std::string* outPath)
{
    if (!outPath || !extension || weapon <= 0) {
        return false;
    }

    std::string humanOverlayRoot;
    std::string jobStem;
    std::string sexToken;
    if (!ExtractPcOverlayPathParts(bodyActName, &humanOverlayRoot, &jobStem, &sexToken)) {
        return false;
    }

    const int primaryWeaponItemId = weapon & 0xFFFF;
    const int secondaryWeaponItemId = static_cast<int>((static_cast<unsigned int>(weapon) >> 16) & 0xFFFFu);
    bool dualWeapon = false;
    const int weaponType = ResolvePcWeaponOverlayType(actor, weapon, &dualWeapon);

    if (weaponType <= 0) {
        return false;
    }

    if (secondLayer && !SupportsPcWeaponOverlaySecondLayer(weaponType)) {
        return false;
    }

    auto buildNumericPath = [&](int numericWeapon) {
        char buffer[512] = {};
        if (secondLayer) {
            std::sprintf(buffer,
                "%s%s_%s_%d_%s.%s",
                humanOverlayRoot.c_str(),
                jobStem.c_str(),
                sexToken.c_str(),
                numericWeapon,
                kWeaponTokenLightSuffix,
                extension);
        } else {
            std::sprintf(buffer,
                "%s%s_%s_%d.%s",
                humanOverlayRoot.c_str(),
                jobStem.c_str(),
                sexToken.c_str(),
                numericWeapon,
                extension);
        }
        return std::string(buffer);
    };

    std::string candidate;
    auto hasSpritePair = [](const std::string& path) {
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos) {
            return false;
        }
        const std::string stem = path.substr(0, dot);
        return g_resMgr.IsExist((stem + ".act").c_str())
            && g_resMgr.IsExist((stem + ".spr").c_str());
    };
    if ((!dualWeapon || secondLayer) && primaryWeaponItemId > 0) {
        candidate = buildNumericPath(primaryWeaponItemId);
        if (hasSpritePair(candidate)) {
            *outPath = candidate;
            return true;
        }
    }

    std::vector<std::string> tokenCandidates;
    if (const std::string overlayToken = GetPcOverlayWeaponToken(weaponType); !overlayToken.empty()) {
        tokenCandidates.push_back(overlayToken);
    }
    if (const std::string weaponTokenFromLua = g_session.GetPlayerWeaponToken(weaponType); !weaponTokenFromLua.empty()) {
        if (std::find(tokenCandidates.begin(), tokenCandidates.end(), weaponTokenFromLua) == tokenCandidates.end()) {
            tokenCandidates.push_back(weaponTokenFromLua);
        }
    }
    if (tokenCandidates.empty()) {
        return false;
    }

    for (const std::string& token : tokenCandidates) {
        char buffer[512] = {};
        if (secondLayer) {
            std::sprintf(buffer,
                "%s%s_%s_%s_%s.%s",
                humanOverlayRoot.c_str(),
                jobStem.c_str(),
                sexToken.c_str(),
                token.c_str(),
                kWeaponTokenLightSuffix,
                extension);
        } else {
            std::sprintf(buffer,
                "%s%s_%s_%s.%s",
                humanOverlayRoot.c_str(),
                jobStem.c_str(),
                sexToken.c_str(),
                token.c_str(),
                extension);
        }

        candidate.assign(buffer);
        if (hasSpritePair(candidate)) {
            *outPath = candidate;
            return true;
        }
    }

    return false;
}

bool BuildWeaponOverlayPath(const CGameActor& actor,
    const std::string& bodyActName,
    int weapon,
    bool secondLayer,
    const char* extension,
    std::string* outPath)
{
    if (TryBuildWeaponOverlayPath(actor, bodyActName, weapon, secondLayer, extension, outPath)) {
        return true;
    }
    std::string overlayRoot;
    std::string jobStem;
    std::string sexToken;
    if (!ExtractPcOverlayPathParts(bodyActName, &overlayRoot, &jobStem, &sexToken)) {
        return false;
    }
    const size_t fileStart = bodyActName.find_last_of("\\/");
    const std::string bodyRoot = bodyActName.substr(0, fileStart + 1);
    for (const std::string& token : g_session.GetPlayerWeaponJobTokens(actor.m_job)) {
        if (token == jobStem) {
            continue;
        }
        // Transcendent/baby bodies commonly share their base class's weapon sprites.
        const std::string fallbackBody = bodyRoot + token + "_" + sexToken + ".act";
        if (TryBuildWeaponOverlayPath(actor, fallbackBody, weapon, secondLayer, extension, outPath)) {
            return true;
        }
    }
    return false;
}

bool BuildShieldOverlayPath(const std::string& bodyActName,
    int shield,
    const char* extension,
    std::string* outPath)
{
    if (!outPath || !extension) {
        return false;
    }

    const int shieldViewId = NormalizeShieldViewId(shield);
    const char* shieldToken = GetShieldToken(shieldViewId);
    if (!shieldToken) {
        return false;
    }

    std::string humanOverlayRoot;
    std::string jobStem;
    std::string sexToken;
    if (!ExtractPcOverlayPathParts(bodyActName, &humanOverlayRoot, &jobStem, &sexToken)) {
        return false;
    }

    char buffer[512] = {};
    std::sprintf(buffer,
        "%s%s\\%s_%s_%s.%s",
        kShieldSpriteRoot,
        jobStem.c_str(),
        jobStem.c_str(),
        sexToken.c_str(),
        shieldToken,
        extension);
    std::string candidate(buffer);
    if (!g_resMgr.IsExist(candidate.c_str())) {
        return false;
    }

    *outPath = candidate;
    return true;
}

CActRes* ResolvePcTransientActRes(const CGameActor& actor, int action)
{
    if (actor.m_isPc == 0) {
        return nullptr;
    }

    const CPc* pcActor = dynamic_cast<const CPc*>(&actor);
    if (!pcActor) {
        return nullptr;
    }

    const int displayJob = ResolveDisplayJob(actor);
    const int sex = actor.m_sex != 0 ? 1 : 0;
    char bodyActPath[260] = {};
    const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyActPath, actor.m_bodyStyle);
    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());

    const bool isAttackLikeState = actor.m_stateId == kAttackStateId
        || actor.m_stateId == kSecondAttackStateId;
    const bool isAttackLikeAction = action >= 80 && action < 96;
    const int weaponValue = ResolvePcAttackWeaponValue(actor);
    if ((!isAttackLikeState && !isAttackLikeAction) || weaponValue == 0) {
        return bodyActRes;
    }

    auto tryOverlayAct = [&](bool secondLayer, std::string* outPath) -> CActRes* {
        std::string overlayActNameResolved;
        if (!BuildWeaponOverlayPath(actor, bodyActName, weaponValue, secondLayer, "act", &overlayActNameResolved)) {
            return nullptr;
        }

        CActRes* overlayActRes = g_resMgr.GetAs<CActRes>(overlayActNameResolved.c_str());
        if (!overlayActRes || ResolveAvailableActionIndex(overlayActRes, action) < 0) {
            return nullptr;
        }

        if (outPath) {
            *outPath = overlayActNameResolved;
        }
        return overlayActRes;
    };

    std::string primaryOverlayActName;
    if (CActRes* primaryOverlayActRes = tryOverlayAct(false, &primaryOverlayActName)) {
        return primaryOverlayActRes;
    }

    std::string secondaryOverlayActName;
    if (CActRes* secondaryOverlayActRes = tryOverlayAct(true, &secondaryOverlayActName)) {
        const bool isLocalPlayer = actor.m_gid != 0 && (actor.m_gid == g_session.m_gid || actor.m_gid == g_session.m_aid);
        if (kLogBillboardDiagnostics && isLocalPlayer) {
            static std::map<u32, std::string> loggedSecondaryAttackActs;
            const std::string logKey = std::to_string(action) + ":" + secondaryOverlayActName;
            if (loggedSecondaryAttackActs[actor.m_gid] != logKey) {
                loggedSecondaryAttackActs[actor.m_gid] = logKey;
                DbgLog("[ActorTrace] TransientAct gid=%u state=%d action=%d weaponValue=%d primaryAct='%s' secondaryAct='%s' selected='secondary'\n",
                    actor.m_gid,
                    actor.m_stateId,
                    action,
                    weaponValue,
                    primaryOverlayActName.c_str(),
                    secondaryOverlayActName.c_str());
            }
        }
        return secondaryOverlayActRes;
    }

    return bodyActRes;
}

int ResolveOverlayMotionIndex(CActRes* actRes, int curAction, int curMotion, const std::string& bodyActName)
{
    if (!actRes) {
        return curMotion;
    }

    int motionIndex = curMotion;
    const int overlayMotionCount = actRes->GetMotionCount(curAction);
    if (overlayMotionCount <= 0) {
        return 0;
    }

    CActRes* bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
    const int bodyMotionCount = bodyActRes ? bodyActRes->GetMotionCount(curAction) : 0;
    if (bodyMotionCount > 0
        && overlayMotionCount > bodyMotionCount
        && (overlayMotionCount % bodyMotionCount) == 0) {
        motionIndex = curMotion * (overlayMotionCount / bodyMotionCount);
    }

    return (std::max)(0, (std::min)(motionIndex, overlayMotionCount - 1));
}

bool DrawPlayerOverlayMotion(BillboardComposeSurface& bitmap,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& bodyActName,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName)
{
    if (actName.empty() || sprName.empty()) {
        return false;
    }

    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        return false;
    }

    const int motionIndex = ResolveOverlayMotionIndex(actRes, curAction, curMotion, bodyActName);
    const CMotion* motion = actRes->GetMotion(curAction, motionIndex);
    if (!motion) {
        motion = actRes->GetMotion(curAction, 0);
    }
    if (!motion) {
        return false;
    }

    const POINT point = imfRes->GetPoint(layerIndex, curAction, curMotion);
    const auto& sample = bitmap.animation.body;
    const int nextIndex = ResolveOverlayMotionIndex(actRes, curAction, sample.next, bodyActName);
    const CMotion* nextMotion = actRes->GetMotion(curAction, nextIndex);
    const POINT nextPoint = imfRes->GetPoint(layerIndex, curAction, sample.next);
    bitmap.BltInterpolatedSprite(static_cast<float>(drawX + point.x) + (nextPoint.x - point.x) * sample.Fraction(),
        static_cast<float>(drawY + point.y) + (nextPoint.y - point.y) * sample.Fraction(),
        sprRes, motion, nextMotion, sprRes->m_pal, sample.Fraction());
    return true;
}

std::array<int, 8> BuildPlayerRenderLayerOrder(CImfRes* imfRes, int curAction, int curMotion)
{
    std::array<int, 8> order{};
    if (!imfRes) {
        order = { 7, 0, 1, 4, 3, 2, 5, 6 };
        return order;
    }

    auto resolveLayerPriority = [&](int priority) {
        int layer = imfRes->GetLayer(priority, curAction, curMotion);
        if (layer < 0) {
            layer = priority;
        }
        return layer;
    };

    const int dir = curAction & 7;
    bool headLayerPassed = false;
    int bodyAndAccessoryIsExchanged = 0;
    int outIndex = 0;

    for (int pass = 7; pass >= 0; --pass) {
        int layer = 0;
        if (dir >= 2 && dir <= 5) {
            if (pass == 7) {
                layer = 7;
            } else if (pass >= 5 && pass <= 6) {
                layer = resolveLayerPriority(pass - 5);
            } else {
                layer = 6 - pass;
            }
        } else if (pass >= 6 && pass <= 7) {
            layer = resolveLayerPriority(pass - 6);
        } else {
            layer = 7 - pass;
        }

        const int originalLayer = layer;
        if ((headLayerPassed || (headLayerPassed = layer == 1)) && layer == 0) {
            layer = 2;
            ++bodyAndAccessoryIsExchanged;
        }
        if (bodyAndAccessoryIsExchanged == 1 && originalLayer == 2) {
            bodyAndAccessoryIsExchanged = 2;
            layer = 0;
        }
        if (layer >= 8) {
            layer = 0;
        }
        if (layer == 2) {
            layer = 4;
        } else if (layer == 4) {
            layer = 2;
        }

        order[outIndex++] = layer;
    }

    int bodyIndex = -1;
    int headIndex = -1;
    for (int index = 0; index < static_cast<int>(order.size()); ++index) {
        if (order[index] == 0 && bodyIndex < 0) {
            bodyIndex = index;
        } else if (order[index] == 1 && headIndex < 0) {
            headIndex = index;
        }
    }

    if (bodyIndex >= 0 && headIndex >= 0 && headIndex < bodyIndex) {
        std::swap(order[bodyIndex], order[headIndex]);
    }

    auto isHeadAccessoryLayer = [](int layer) {
        return layer == 2 || layer == 3 || layer == 4;
    };

    std::array<int, 8> reordered{};
    int delayedAccessories[8] = {};
    int delayedAccessoryCount = 0;
    int reorderedCount = 0;
    bool headDrawn = false;

    for (int layer : order) {
        if (!headDrawn && isHeadAccessoryLayer(layer)) {
            delayedAccessories[delayedAccessoryCount++] = layer;
            continue;
        }

        reordered[reorderedCount++] = layer;
        if (layer == 1) {
            headDrawn = true;
            for (int index = 0; index < delayedAccessoryCount; ++index) {
                reordered[reorderedCount++] = delayedAccessories[index];
            }
            delayedAccessoryCount = 0;
        }
    }

    for (int index = 0; index < delayedAccessoryCount; ++index) {
        reordered[reorderedCount++] = delayedAccessories[index];
    }

    order = reordered;

    return order;
}

bool DrawPlayerLayer(BillboardComposeSurface& bitmap,
    int drawX,
    int drawY,
    int layerIndex,
    int curAction,
    int curMotion,
    const std::string& actName,
    const std::string& sprName,
    const std::string& imfName,
    const std::string& bodyActName,
    const std::string& headActName,
    int bodyMotionIndex,
    int headMotionIndex,
    const std::string& paletteName)
{
    CActRes* actRes = g_resMgr.GetAs<CActRes>(actName.c_str());
    CSprRes* sprRes = g_resMgr.GetAs<CSprRes>(sprName.c_str());
    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfName.c_str());
    if (!actRes || !sprRes || !imfRes) {
        static std::map<std::string, bool> loggedMissingResources;
        const std::string key = actName + "|" + sprName + "|" + imfName;
        if (loggedMissingResources.insert(std::make_pair(key, true)).second) {
            DbgLog("[Actor] player layer resources missing act='%s' actExists=%d actRes=%p spr='%s' sprExists=%d sprRes=%p imf='%s' imfExists=%d imfRes=%p\n",
                actName.c_str(),
                g_resMgr.IsExist(actName.c_str()) ? 1 : 0,
                actRes,
                sprName.c_str(),
                g_resMgr.IsExist(sprName.c_str()) ? 1 : 0,
                sprRes,
                imfName.c_str(),
                g_resMgr.IsExist(imfName.c_str()) ? 1 : 0,
                imfRes);
        }
        return false;
    }

    int resolvedLayer = imfRes->GetLayer(layerIndex, curAction, curMotion);
    if (resolvedLayer < 0) {
        resolvedLayer = layerIndex;
    }

    const CMotion* motion = actRes->GetMotion(curAction, curMotion);
    if (!motion || resolvedLayer >= static_cast<int>(motion->sprClips.size())) {
        static std::map<std::string, bool> loggedMotionFailures;
        char key[512] = {};
        std::sprintf(key, "%s|layer=%d|action=%d|motion=%d|resolved=%d", actName.c_str(), layerIndex, curAction, curMotion, resolvedLayer);
        if (loggedMotionFailures.insert(std::make_pair(key, true)).second) {
            DbgLog("[Actor] player layer motion missing act='%s' layer=%d action=%d motion=%d resolvedLayer=%d clipCount=%d\n",
                actName.c_str(),
                layerIndex,
                curAction,
                curMotion,
                resolvedLayer,
                motion ? static_cast<int>(motion->sprClips.size()) : -1);
        }
        return false;
    }

    const POINT point = GetPlayerLayerPoint(layerIndex,
        resolvedLayer,
        imfRes,
        motion,
        bodyActName,
        headActName,
        curAction,
        bodyMotionIndex,
        curMotion,
        headMotionIndex);

    std::array<unsigned int, 256> paletteOverride{};
    unsigned int* palette = sprRes->m_pal;
    if (BuildPaletteOverride(paletteName, paletteOverride)) {
        palette = paletteOverride.data();
    }

    CMotion singleLayerMotion{};
    singleLayerMotion.sprClips.push_back(motion->sprClips[resolvedLayer]);
    CMotion nextLayerMotion = singleLayerMotion;
    POINT nextPoint = point;
    const auto& sample = bitmap.animation.body;
    const int nextMotionIndex = sample.next;
    const CMotion* nextMotion = actRes->GetMotion(curAction, nextMotionIndex);
    int nextLayer = imfRes->GetLayer(layerIndex, curAction, nextMotionIndex);
    if (nextLayer < 0) nextLayer = layerIndex;
    if (nextMotion && nextLayer >= 0 && nextLayer < static_cast<int>(nextMotion->sprClips.size())) {
        nextLayerMotion.sprClips[0] = nextMotion->sprClips[nextLayer];
        nextPoint = GetPlayerLayerPoint(layerIndex, nextLayer, imfRes, nextMotion,
            bodyActName, headActName, curAction, sample.next, nextMotionIndex, sample.next);
    }
    bitmap.BltInterpolatedSprite(static_cast<float>(drawX + point.x) + (nextPoint.x - point.x) * sample.Fraction(),
        static_cast<float>(drawY + point.y) + (nextPoint.y - point.y) * sample.Fraction(),
        sprRes, &singleLayerMotion, &nextLayerMotion, palette, sample.Fraction());
    return true;
}

bool DrawPcBillboard(BillboardComposeSurface& bitmap,
    const CPc& actor,
    int drawX,
    int drawY,
    int bodyAction,
    int headMotion,
    const std::array<int, 3>& accessoryMotions,
    int* outJob,
    int* outHead,
    int* outSex,
    int* outBodyPalette,
    int* outHeadPalette)
{
    char bodyAct[260] = {};
    char bodySpr[260] = {};
    char headAct[260] = {};
    char headSpr[260] = {};
    char accessoryBottomAct[260] = {};
    char accessoryBottomSpr[260] = {};
    char accessoryMidAct[260] = {};
    char accessoryMidSpr[260] = {};
    char accessoryTopAct[260] = {};
    char accessoryTopSpr[260] = {};
    char imfName[260] = {};
    char bodyPalette[260] = {};
    char headPalette[260] = {};

    const int displayJob = ResolveDisplayJob(actor);
    const int sex = actor.m_sex != 0 ? 1 : 0;
    int head = actor.m_head;
    const int curAction = bodyAction;
    const int curMotion = headMotion;
    const int weaponValue = ResolvePcAttackWeaponValue(actor);

    const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyAct, actor.m_bodyStyle);
    const std::string bodySprName = g_session.GetJobSprName(displayJob, sex, bodySpr, actor.m_bodyStyle);
    const std::string headActName = g_session.GetHeadActName(displayJob, &head, sex, headAct);
    const std::string headSprName = g_session.GetHeadSprName(displayJob, &head, sex, headSpr);
    const std::string accessoryBottomActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory, accessoryBottomAct);
    const std::string accessoryBottomSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory, accessoryBottomSpr);
    const std::string accessoryMidActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory3, accessoryMidAct);
    const std::string accessoryMidSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory3, accessoryMidSpr);
    const std::string accessoryTopActName = g_session.GetAccessoryActName(displayJob, &head, sex, actor.m_accessory2, accessoryTopAct);
    const std::string accessoryTopSprName = g_session.GetAccessorySprName(displayJob, &head, sex, actor.m_accessory2, accessoryTopSpr);
    const std::string imfPath = g_session.GetImfName(displayJob, head, sex, imfName);
    const std::string bodyPaletteName = actor.m_bodyPalette > 0
        ? g_session.GetBodyPaletteName(displayJob, sex, actor.m_bodyPalette, bodyPalette, actor.m_bodyStyle)
        : std::string();
    const std::string headPaletteName = actor.m_headPalette > 0
        ? g_session.GetHeadPaletteName(head, displayJob, sex, actor.m_headPalette, headPalette)
        : std::string();

    std::string weaponActNameResolved;
    std::string weaponSprNameResolved;
    std::string weaponLayer6ActNameResolved;
    std::string weaponLayer6SprNameResolved;
    std::string shieldActNameResolved;
    std::string shieldSprNameResolved;
    BuildWeaponOverlayPath(actor, bodyActName, weaponValue, false, "act", &weaponActNameResolved);
    BuildWeaponOverlayPath(actor, bodyActName, weaponValue, false, "spr", &weaponSprNameResolved);
    BuildWeaponOverlayPath(actor, bodyActName, weaponValue, true, "act", &weaponLayer6ActNameResolved);
    BuildWeaponOverlayPath(actor, bodyActName, weaponValue, true, "spr", &weaponLayer6SprNameResolved);
    BuildShieldOverlayPath(bodyActName, actor.m_shield, "act", &shieldActNameResolved);
    BuildShieldOverlayPath(bodyActName, actor.m_shield, "spr", &shieldSprNameResolved);
    if (actor.m_bodyStyle > 0 && (shieldActNameResolved.empty() || shieldSprNameResolved.empty())) {
        char originalBodyAct[260] = {};
        const std::string originalBodyActName = g_session.GetJobActName(displayJob, sex, originalBodyAct);
        BuildShieldOverlayPath(originalBodyActName, actor.m_shield, "act", &shieldActNameResolved);
        BuildShieldOverlayPath(originalBodyActName, actor.m_shield, "spr", &shieldSprNameResolved);
    }

    const bool isLocalPlayer = actor.m_gid != 0
        && (actor.m_gid == g_session.m_gid || actor.m_gid == g_session.m_aid);
    if (kLogBillboardDiagnostics && isLocalPlayer) {
        static std::map<u32, std::string> loggedBillboardResources;
        char logKey[2048] = {};
        std::snprintf(logKey,
            sizeof(logKey),
            "%d|%d|%d|%d|%d|%s|%s|%s|%s|%s|%s|%d|%d|%d|%d",
            displayJob,
            sex,
            bodyAction,
            headMotion,
            actor.m_stateId,
            bodyActName.c_str(),
            bodySprName.c_str(),
            imfPath.c_str(),
            bodyPaletteName.c_str(),
            headPaletteName.c_str(),
            weaponActNameResolved.c_str(),
            actor.m_bodyPalette,
            actor.m_headPalette,
            actor.m_weapon,
            actor.m_shield);
        const std::string logValue(logKey);
        if (loggedBillboardResources[actor.m_gid] != logValue) {
            loggedBillboardResources[actor.m_gid] = logValue;
            DbgLog("[ActorTrace] pc billboard resources gid=%u displayJob=%d sex=%d state=%d curAction=%d curMotion=%d bodyAction=%d headMotion=%d bodyAct='%s' bodySpr='%s' headAct='%s' headSpr='%s' imf='%s' bodyPal='%s' headPal='%s' bodyPalId=%d headPalId=%d accessoryBottom=%d accessoryMid=%d accessoryTop=%d weapon=%d shield=%d weaponAct='%s' weaponSpr='%s' weapon2Act='%s' weapon2Spr='%s' shieldAct='%s' shieldSpr='%s'\n",
                actor.m_gid,
                displayJob,
                sex,
                actor.m_stateId,
                actor.m_curAction,
                actor.m_curMotion,
                bodyAction,
                headMotion,
                bodyActName.c_str(),
                bodySprName.c_str(),
                headActName.c_str(),
                headSprName.c_str(),
                imfPath.c_str(),
                bodyPaletteName.c_str(),
                headPaletteName.c_str(),
                actor.m_bodyPalette,
                actor.m_headPalette,
                actor.m_accessory,
                actor.m_accessory3,
                actor.m_accessory2,
                actor.m_weapon,
                actor.m_shield,
                weaponActNameResolved.c_str(),
                weaponSprNameResolved.c_str(),
                weaponLayer6ActNameResolved.c_str(),
                weaponLayer6SprNameResolved.c_str(),
                shieldActNameResolved.c_str(),
                shieldSprNameResolved.c_str());
        }

        if (actor.m_bodyPalette > 0) {
            const int basePaletteJob = ResolveClassicTransBaseJobForComparison(displayJob);
            if (basePaletteJob != displayJob) {
                char baseBodyPalette[260] = {};
                const std::string baseBodyPaletteName = g_session.GetBodyPaletteName(basePaletteJob, sex, actor.m_bodyPalette, baseBodyPalette);

                BillboardComposeSurface actualSurface(kPlayerBillboardComposeWidth, kPlayerBillboardComposeHeight);
                actualSurface.Clear(0x00000000u);
                BillboardComposeSurface baseSurface(kPlayerBillboardComposeWidth, kPlayerBillboardComposeHeight);
                baseSurface.Clear(0x00000000u);

                const bool actualLayerOk = DrawPlayerLayer(actualSurface,
                    kPlayerBillboardAnchorX,
                    kPlayerBillboardAnchorY,
                    0,
                    curAction,
                    curMotion,
                    bodyActName,
                    bodySprName,
                    imfPath,
                    bodyActName,
                    headActName,
                    curMotion,
                    headMotion,
                    bodyPaletteName);
                const bool baseLayerOk = DrawPlayerLayer(baseSurface,
                    kPlayerBillboardAnchorX,
                    kPlayerBillboardAnchorY,
                    0,
                    curAction,
                    curMotion,
                    bodyActName,
                    bodySprName,
                    imfPath,
                    bodyActName,
                    headActName,
                    curMotion,
                    headMotion,
                    baseBodyPaletteName);

                const u32 actualHash = actualLayerOk
                    ? HashPixelBuffer(actualSurface.GetImageData(), static_cast<size_t>(actualSurface.GetWidth()) * static_cast<size_t>(actualSurface.GetHeight()))
                    : 0u;
                const u32 baseHash = baseLayerOk
                    ? HashPixelBuffer(baseSurface.GetImageData(), static_cast<size_t>(baseSurface.GetWidth()) * static_cast<size_t>(baseSurface.GetHeight()))
                    : 0u;

                static std::map<u32, std::string> loggedBodyPaletteComparisons;
                char compareKey[1024] = {};
                std::snprintf(compareKey,
                    sizeof(compareKey),
                    "%d|%d|%d|%d|%u|%u|%s|%s",
                    displayJob,
                    basePaletteJob,
                    curAction,
                    curMotion,
                    actualHash,
                    baseHash,
                    bodyPaletteName.c_str(),
                    baseBodyPaletteName.c_str());
                const std::string compareValue(compareKey);
                if (loggedBodyPaletteComparisons[actor.m_gid] != compareValue) {
                    loggedBodyPaletteComparisons[actor.m_gid] = compareValue;
                    DbgLog("[ActorTrace] body palette compare gid=%u displayJob=%d baseJob=%d pal=%d action=%d motion=%d actualPal='%s' basePal='%s' actualOk=%d baseOk=%d actualHash=%08X baseHash=%08X samePixels=%d\n",
                        actor.m_gid,
                        displayJob,
                        basePaletteJob,
                        actor.m_bodyPalette,
                        curAction,
                        curMotion,
                        bodyPaletteName.c_str(),
                        baseBodyPaletteName.c_str(),
                        actualLayerOk ? 1 : 0,
                        baseLayerOk ? 1 : 0,
                        static_cast<unsigned int>(actualHash),
                        static_cast<unsigned int>(baseHash),
                        actualLayerOk && baseLayerOk && actualHash == baseHash ? 1 : 0);
                }
            }
        }
    }

    CImfRes* imfRes = g_resMgr.GetAs<CImfRes>(imfPath.c_str());
    const std::array<int, 8> layerOrder = BuildPlayerRenderLayerOrder(imfRes, curAction, curMotion);

    bool bodyOk = false;
    bool headOk = false;
    bool accessoryBottomOk = false;
    bool accessoryMidOk = false;
    bool accessoryTopOk = false;
    bool weaponOk = false;
    bool weaponLayer6Ok = false;
    bool shieldOk = false;

    for (int layer : layerOrder) {
        switch (layer) {
        case 0:
            bodyOk |= DrawPlayerLayer(bitmap, drawX, drawY, 0, curAction, curMotion, bodyActName, bodySprName, imfPath, bodyActName, headActName, curMotion, headMotion, bodyPaletteName);
            break;
        case 1:
            headOk |= DrawPlayerLayer(bitmap, drawX, drawY, 1, curAction, headMotion, headActName, headSprName, imfPath, bodyActName, headActName, curMotion, headMotion, headPaletteName);
            break;
        case 2:
            accessoryBottomOk |= !accessoryBottomActName.empty() && !accessoryBottomSprName.empty()
                ? DrawAttachedAccessoryMotion(bitmap, drawX, drawY, curAction, curMotion, headMotion, accessoryMotions[0], bodyActName, headActName, accessoryBottomActName, accessoryBottomSprName, 0)
                : false;
            break;
        case 3:
            accessoryMidOk |= !accessoryMidActName.empty() && !accessoryMidSprName.empty()
                ? DrawAttachedAccessoryMotion(bitmap, drawX, drawY, curAction, curMotion, headMotion, accessoryMotions[1], bodyActName, headActName, accessoryMidActName, accessoryMidSprName, 1)
                : false;
            break;
        case 4:
            accessoryTopOk |= !accessoryTopActName.empty() && !accessoryTopSprName.empty()
                ? DrawAttachedAccessoryMotion(bitmap, drawX, drawY, curAction, curMotion, headMotion, accessoryMotions[2], bodyActName, headActName, accessoryTopActName, accessoryTopSprName, 2)
                : false;
            break;
        case 5:
            weaponOk |= DrawPlayerOverlayMotion(bitmap, drawX, drawY, 5, curAction, curMotion, bodyActName, weaponActNameResolved, weaponSprNameResolved, imfPath);
            break;
        case 6:
            weaponLayer6Ok |= DrawPlayerOverlayMotion(bitmap, drawX, drawY, 6, curAction, curMotion, bodyActName, weaponLayer6ActNameResolved, weaponLayer6SprNameResolved, imfPath);
            break;
        case 7:
            shieldOk |= DrawPlayerOverlayMotion(bitmap, drawX, drawY, 7, curAction, curMotion, bodyActName, shieldActNameResolved, shieldSprNameResolved, imfPath);
            break;
        default:
            break;
        }
    }

    if (!bodyOk && !headOk && !accessoryBottomOk && !accessoryMidOk && !accessoryTopOk && !weaponOk && !weaponLayer6Ok && !shieldOk) {
        static std::map<int, bool> loggedBillboardFailures;
        if (loggedBillboardFailures.insert(std::make_pair(displayJob, true)).second) {
            DbgLog("[Actor] player billboard draw failed job=%d bodyAct='%s' bodySpr='%s' headAct='%s' headSpr='%s' weaponAct='%s' weaponLayer6Act='%s' shieldAct='%s' imf='%s' bodyPal='%s' headPal='%s' action=%d motion=%d head=%d sex=%d weaponValue=%d weapon=%d shield=%d\n",
                displayJob,
                bodyActName.c_str(),
                bodySprName.c_str(),
                headActName.c_str(),
                headSprName.c_str(),
                weaponActNameResolved.c_str(),
                weaponLayer6ActNameResolved.c_str(),
                shieldActNameResolved.c_str(),
                imfPath.c_str(),
                bodyPaletteName.c_str(),
                headPaletteName.c_str(),
                curAction,
                curMotion,
                head,
                sex,
                weaponValue,
                actor.m_weapon,
                actor.m_shield);
        }
    }

    if ((actor.m_stateId == kAttackStateId || actor.m_stateId == kSecondAttackStateId) && actor.m_isPc != 0) {
        static std::map<u32, u32> loggedAttackRenderState;
        const u32 renderKey = (static_cast<u32>(curAction & 0xFFFF) << 16) | static_cast<u32>(curMotion & 0xFFFF);
        if (loggedAttackRenderState[actor.m_gid] != renderKey) {
            loggedAttackRenderState[actor.m_gid] = renderKey;
            DbgLog("[Actor] pc attack render gid=%u state=%d action=%d motion=%d roty=%.1f dir=%d weaponValue=%d weapon=%d shield=%d bodyAct='%s' weaponAct='%s' weaponLayer6Act='%s' weaponLayer6Spr='%s'\n",
                actor.m_gid,
                actor.m_stateId,
                curAction,
                curMotion,
                actor.m_roty,
                const_cast<CPc&>(actor).Get8Dir(actor.m_roty),
                weaponValue,
                actor.m_weapon,
                actor.m_shield,
                bodyActName.c_str(),
                weaponActNameResolved.c_str(),
                weaponLayer6ActNameResolved.c_str(),
                weaponLayer6SprNameResolved.c_str());
        }
    }

    if (outJob) {
        *outJob = displayJob;
    }
    if (outHead) {
        *outHead = head;
    }
    if (outSex) {
        *outSex = sex;
    }
    if (outBodyPalette) {
        *outBodyPalette = actor.m_bodyPalette;
    }
    if (outHeadPalette) {
        *outHeadPalette = actor.m_headPalette;
    }

    return bodyOk || headOk || accessoryBottomOk || accessoryMidOk || accessoryTopOk || weaponOk || weaponLayer6Ok || shieldOk;
}

bool ResolveNonPcSpritePaths(int job, char* actPath, char* sprPath)
{
    if (!actPath || !sprPath) {
        return false;
    }

    const char* spriteName = g_session.GetNonPcSpriteName(job);
    if (!spriteName) {
        spriteName = ResolveNonPcSpriteAlias(job, g_session.GetJobName(job));
    }
    if (!spriteName || !*spriteName) {
        return false;
    }

    if (job >= 1000) {
        if (job >= 6001 && job <= 6047) {
            const char* const spriteRoot = (job >= 6017 && job <= 6046)
                ? "data\\sprite\\mercenary\\"
                : "data\\sprite\\homun\\";
            std::sprintf(actPath, "%s%s.act", spriteRoot, spriteName);
            std::sprintf(sprPath, "%s%s.spr", spriteRoot, spriteName);
            return true;
        }

        if (TryResolveNonPcSpritePaths("data\\sprite\\monster\\", spriteName, actPath, sprPath)) {
            return true;
        }
        if (TryResolveNonPcSpritePaths(kLegacyMonsterSpriteRoot, spriteName, actPath, sprPath)) {
            return true;
        }
        // Modern NPC view IDs also occupy the range above 1000.
        if (TryResolveNonPcSpritePaths("data\\sprite\\NPC\\", spriteName, actPath, sprPath)) {
            return true;
        }
        if (TryResolveNonPcSpritePaths("data\\sprite\\", spriteName, actPath, sprPath)) {
            return true;
        }

        std::sprintf(actPath, "%s%s.act", "data\\sprite\\monster\\", spriteName);
        std::sprintf(sprPath, "%s%s.spr", "data\\sprite\\monster\\", spriteName);
        return true;
    }

    std::sprintf(actPath, "%s%s.act", "data\\sprite\\NPC\\", spriteName);
    std::sprintf(sprPath, "%s%s.spr", "data\\sprite\\NPC\\", spriteName);
    return true;
}

void LogNonPcResourceResolvedOnce(int job, const char* jobName, const char* actPath, const char* sprPath)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc resources resolved job=%d name='%s' act='%s' spr='%s'\n",
        job,
        jobName ? jobName : "",
        actPath ? actPath : "",
        sprPath ? sprPath : "");
}

void LogNonPcResourceMissingOnce(int job, const char* jobName, const char* actPath, const char* sprPath, const void* actRes, const void* sprRes)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc resources missing job=%d name='%s' act='%s' actRes=%p spr='%s' sprRes=%p\n",
        job,
        jobName ? jobName : "",
        actPath ? actPath : "",
        actRes,
        sprPath ? sprPath : "",
        sprRes);
}

void LogNonPcBillboardFailureOnce(int job, const char* jobName, int bodyAction, int motion)
{
    static std::map<int, bool> loggedJobs;
    if (!loggedJobs.insert(std::make_pair(job, true)).second) {
        return;
    }

    DbgLog("[Actor] nonpc billboard draw failed job=%d name='%s' bodyAction=%d motion=%d\n",
        job,
        jobName ? jobName : "",
        bodyAction,
        motion);
}

CActRes* ResolveRuntimeActorActRes(CRenderObject* object);

bool DrawNonPcBillboard(BillboardComposeSurface& bitmap,
    const CPc& actor,
    int drawX,
    int drawY,
    int bodyAction,
    int motion,
    int* outJob)
{
    CActRes* actRes = nullptr;
    CSprRes* sprRes = nullptr;
    const CMotion* spriteMotion = nullptr;
    if (!ResolveNonPcBillboardMotion(actor, bodyAction, motion, &actRes, &sprRes, &spriteMotion)) {
        if (IsPortalFallbackJob(actor.m_job)) {
            return false;
        }
        return DrawWarpPortalFallback(bitmap, actor, outJob);
    }

    if (!spriteMotion) {
        return false;
    }

    const CMotion* nextMotion = spriteMotion;
    CActRes* nextAct = nullptr;
    CSprRes* nextSpr = nullptr;
    if (bitmap.animation.enabled) {
        ResolveNonPcBillboardMotion(actor, bodyAction, bitmap.animation.body.next,
            &nextAct, &nextSpr, &nextMotion);
    }
    bitmap.BltInterpolatedSprite(static_cast<float>(drawX), static_cast<float>(drawY),
        sprRes, spriteMotion, nextMotion, sprRes->m_pal, bitmap.animation.body.Fraction());
    const bool drawOk = true;

    if (outJob) {
        *outJob = ResolveDisplayJob(actor);
    }
    return drawOk;
}

bool PopulateSharedNonPcBillboardFrame(CPc& actor, int displayJob, int bodyAction, int motion,
    const ActorBillboardAnimation& animation)
{
    SharedNonPcBillboardKey frameKey{ displayJob, bodyAction, motion, animation };
    auto& sharedCache = GetSharedNonPcBillboardCache();
    const auto existing = sharedCache.find(frameKey);
    if (existing != sharedCache.end()) {
        existing->second.lastUsed = NextBillboardCacheUse();
        return true;
    }

    BillboardComposeConfig composeConfig{};
    if (!ResolveNonPcBillboardComposeConfig(actor, bodyAction, motion, &composeConfig, animation)) {
        composeConfig.width = kPlayerBillboardComposeWidth;
        composeConfig.height = kPlayerBillboardComposeHeight;
        composeConfig.anchorX = kPlayerBillboardAnchorX;
        composeConfig.anchorY = kPlayerBillboardAnchorY;
    }

    BillboardComposeSurface composeSurface(composeConfig.width, composeConfig.height, animation.sharpPixels);
    composeSurface.animation = animation;
    composeSurface.Clear(0x00000000u);

    int resolvedJob = -1;
    if (!DrawNonPcBillboard(composeSurface,
            actor,
            composeConfig.anchorX,
            composeConfig.anchorY,
            bodyAction,
            motion,
            &resolvedJob)) {
        return false;
    }

    tagRECT opaqueBounds{};
    if (!FindOpaqueBounds(composeSurface.GetImageData(),
            static_cast<int>(composeSurface.GetWidth()),
            static_cast<int>(composeSurface.GetHeight()),
            &opaqueBounds)) {
        return false;
    }
    composeSurface.ToLogicalBounds(&opaqueBounds);

    std::vector<unsigned int> pixels(composeSurface.GetImageData(),
        composeSurface.GetImageData() + static_cast<size_t>(composeSurface.GetWidth()) * static_cast<size_t>(composeSurface.GetHeight()));
    UnpremultiplyPixels(pixels);

    CTexture* texture = new CTexture();
    if (!texture) {
        return false;
    }
    if (!texture->Create(composeSurface.GetWidth(), composeSurface.GetHeight(), PF_A8R8G8B8, false)) {
        delete texture;
        return false;
    }

    SetActorBillboardDebugName(texture,
        actor,
        displayJob,
        bodyAction,
        motion,
        false);
    texture->Update(0,
        0,
        composeSurface.GetWidth(),
        composeSurface.GetHeight(),
        pixels.data(),
        true,
        composeSurface.GetWidth() * static_cast<int>(sizeof(unsigned int)));

    SharedNonPcBillboardValue value{};
    value.texture.reset(texture);
    value.lastUsed = NextBillboardCacheUse();
    value.opaqueBounds = opaqueBounds;
    value.width = composeConfig.width;
    value.height = composeConfig.height;
    value.anchorX = composeConfig.anchorX;
    value.anchorY = composeConfig.anchorY;
    sharedCache[frameKey] = value;
    TrimSharedBillboardCache(sharedCache);
    return true;
}

bool FindOpaqueBounds(const unsigned int* pixels, int width, int height, tagRECT* outBounds)
{
    if (!pixels || width <= 0 || height <= 0 || !outBounds) {
        return false;
    }

    int minX = width;
    int minY = height;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const unsigned int pixel = pixels[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)];
            if ((pixel >> 24) == 0) {
                continue;
            }

            minX = (std::min)(minX, x);
            minY = (std::min)(minY, y);
            maxX = (std::max)(maxX, x);
            maxY = (std::max)(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    outBounds->left = minX;
    outBounds->top = minY;
    outBounds->right = maxX + 1;
    outBounds->bottom = maxY + 1;
    return true;
}

void UnpremultiplyPixels(std::vector<unsigned int>& pixels)
{
    for (unsigned int& pixel : pixels) {
        const unsigned int alpha = (pixel >> 24) & 0xFFu;
        if (alpha == 0u || alpha == 0xFFu) {
            continue;
        }

        unsigned int red = (pixel >> 16) & 0xFFu;
        unsigned int green = (pixel >> 8) & 0xFFu;
        unsigned int blue = pixel & 0xFFu;
        red = (std::min)(255u, (red * 255u + alpha / 2u) / alpha);
        green = (std::min)(255u, (green * 255u + alpha / 2u) / alpha);
        blue = (std::min)(255u, (blue * 255u + alpha / 2u) / alpha);
        pixel = (alpha << 24) | (red << 16) | (green << 8) | blue;
    }
}

template <typename T>
bool ReadValue(const unsigned char*& cursor, const unsigned char* end, T& out)
{
    if (cursor + sizeof(T) > end) {
        return false;
    }
    std::memcpy(&out, cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

bool ReadBytes(const unsigned char*& cursor, const unsigned char* end, void* out, size_t size)
{
    if (cursor + size > end) {
        return false;
    }
    std::memcpy(out, cursor, size);
    cursor += size;
    return true;
}

struct ActHeader {
    unsigned short id;
    unsigned short ver;
    unsigned short count;
    unsigned char reserved[10];
};

struct ActClipV1 {
    int x;
    int y;
    int sprIndex;
    int flags;
};

struct ActClipV2 {
    int x;
    int y;
    int sprIndex;
    int flags;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    float zoom;
    int angle;
    int clipType;
};

struct ActClipV4 {
    int x;
    int y;
    int sprIndex;
    int flags;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    float zoomX;
    float zoomY;
    int angle;
    int clipType;
};

struct ActClipV5Extra {
    int width;
    int height;
};

struct ActAttachPoint {
    int reserved;
    int x;
    int y;
    int attr;
};

} // namespace

CActRes::CActRes() : numMaxClipPerMotion(0) {}
CActRes::~CActRes() { Reset(); }

bool CActRes::LoadFromBuffer(const char* fName, const unsigned char* buffer, int size)
{
    LOG_ACT_LOAD("[ActLoad] LoadFromBuffer: '%s' size=%d\n", fName ? fName : "(null)", size);
    (void)fName;

    LOG_ACT_LOAD("[ActLoad] Calling Reset...\n");
    Reset();
    LOG_ACT_LOAD("[ActLoad] Reset done\n");
    if (!buffer || size < static_cast<int>(sizeof(ActHeader))) {
        LOG_ACT_LOAD("[ActLoad] FAIL: null/short buffer\n");
        return false;
    }

    const unsigned char* cursor = buffer;
    const unsigned char* end = buffer + size;

    ActHeader header{};
    if (!ReadValue(cursor, end, header)) {
        LOG_ACT_LOAD("[ActLoad] FAIL: could not read header\n");
        return false;
    }
    LOG_ACT_LOAD("[ActLoad] id=0x%04X ver=0x%04X count=%u\n", header.id, header.ver, header.count);
    if (header.id != 17217 || header.ver > 0x205u) {
        LOG_ACT_LOAD("[ActLoad] FAIL: bad id or ver\n");
        return false;
    }

    LOG_ACT_LOAD("[ActLoad] actions.resize(%u)...\n", header.count);
    actions.resize(header.count);
    LOG_ACT_LOAD("[ActLoad] m_delay.assign(%u)...\n", header.count);
    m_delay.assign(header.count, 4.0f);
    LOG_ACT_LOAD("[ActLoad] Starting action parse loop\n");

    for (unsigned int actionIndex = 0; actionIndex < header.count; ++actionIndex) {
        unsigned int motionCount = 0;
        if (!ReadValue(cursor, end, motionCount)) {
            LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] could not read motionCount\n", actionIndex);
            Reset();
            return false;
        }
        LOG_ACT_LOAD("[ActLoad] action[%u] motionCount=%u\n", actionIndex, motionCount);

        CAction& action = actions[actionIndex];
        action.motions.resize(motionCount);
        for (unsigned int motionIndex = 0; motionIndex < motionCount; ++motionIndex) {
            CMotion& motion = action.motions[motionIndex];
            if (!ReadBytes(cursor, end, &motion.range1, sizeof(RECT)) ||
                !ReadBytes(cursor, end, &motion.range2, sizeof(RECT))) {
                LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] range read failed\n", actionIndex, motionIndex);
                Reset();
                return false;
            }

            unsigned int spriteCount = 0;
            if (!ReadValue(cursor, end, spriteCount)) {
                LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] spriteCount read failed\n", actionIndex, motionIndex);
                Reset();
                return false;
            }
            LOG_ACT_LOAD("[ActLoad] action[%u] motion[%u] spriteCount=%u\n", actionIndex, motionIndex, spriteCount);

            motion.sprClips.resize(spriteCount);
            numMaxClipPerMotion = (std::max)(numMaxClipPerMotion, static_cast<int>(spriteCount));

            for (unsigned int clipIndex = 0; clipIndex < spriteCount; ++clipIndex) {
                CSprClip& clip = motion.sprClips[clipIndex];
                clip.r = 0xFF;
                clip.g = 0xFF;
                clip.b = 0xFF;
                clip.a = 0xFF;
                clip.zoomX = 1.0f;
                clip.zoomY = 1.0f;
                clip.angle = 0;
                clip.clipType = 0;

                if (header.ver >= 0x204u) {
                    ActClipV4 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V4 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                    clip.r = src.r;
                    clip.g = src.g;
                    clip.b = src.b;
                    clip.a = src.a;
                    clip.zoomX = src.zoomX;
                    clip.zoomY = src.zoomY;
                    clip.angle = src.angle;
                    clip.clipType = src.clipType;

                    if (header.ver >= 0x205u) {
                        ActClipV5Extra extra{};
                        if (!ReadValue(cursor, end, extra)) {
                            LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V5 extra read failed\n", clipIndex);
                            Reset();
                            return false;
                        }
                    }
                } else if (header.ver >= 0x200u) {
                    ActClipV2 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V2 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                    clip.r = src.r;
                    clip.g = src.g;
                    clip.b = src.b;
                    clip.a = src.a;
                    clip.zoomX = src.zoom;
                    clip.zoomY = src.zoom;
                    clip.angle = src.angle;
                    clip.clipType = src.clipType;
                } else {
                    ActClipV1 src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: clip[%u] V1 read failed\n", clipIndex);
                        Reset();
                        return false;
                    }
                    clip.x = src.x;
                    clip.y = src.y;
                    clip.sprIndex = src.sprIndex;
                    clip.flags = src.flags;
                }
            }

            motion.eventId = -1;
            if (header.ver >= 0x200u) {
                if (!ReadValue(cursor, end, motion.eventId)) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] eventId read failed\n", actionIndex, motionIndex);
                    Reset();
                    return false;
                }
                if (header.ver == 0x200u) {
                    motion.eventId = -1;
                }
            }

            motion.attachCount = 0;
            motion.attachInfo.clear();
            if (header.ver >= 0x203u) {
                if (!ReadValue(cursor, end, motion.attachCount)) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] attachCount read failed\n", actionIndex, motionIndex);
                    Reset();
                    return false;
                }
                if (motion.attachCount < 0) {
                    LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] negative attachCount=%d\n", actionIndex, motionIndex, motion.attachCount);
                    Reset();
                    return false;
                }

                motion.attachInfo.resize(static_cast<size_t>(motion.attachCount));
                for (int attachIndex = 0; attachIndex < motion.attachCount; ++attachIndex) {
                    ActAttachPoint src{};
                    if (!ReadValue(cursor, end, src)) {
                        LOG_ACT_LOAD("[ActLoad] FAIL: action[%u] motion[%u] attach[%d] read failed\n", actionIndex, motionIndex, attachIndex);
                        Reset();
                        return false;
                    }

                    CAttachPointInfo& dst = motion.attachInfo[static_cast<size_t>(attachIndex)];
                    dst.x = src.x;
                    dst.y = src.y;
                    dst.attr = src.attr;
                }
            }
        }
    }

    if (header.ver >= 0x201u) {
        unsigned int eventCount = 0;
        if (!ReadValue(cursor, end, eventCount)) {
            LOG_ACT_LOAD("[ActLoad] FAIL: eventCount read failed\n");
            Reset();
            return false;
        }
        LOG_ACT_LOAD("[ActLoad] eventCount=%u\n", eventCount);
        m_events.resize(eventCount);
        for (unsigned int i = 0; i < eventCount; ++i) {
            char eventName[40] = {};
            if (!ReadBytes(cursor, end, eventName, sizeof(eventName))) {
                LOG_ACT_LOAD("[ActLoad] FAIL: event[%u] name read failed\n", i);
                Reset();
                return false;
            }
            m_events[i] = eventName;
        }
    }

    if (header.ver >= 0x202u) {
        for (unsigned int i = 0; i < header.count; ++i) {
            if (!ReadValue(cursor, end, m_delay[i])) {
                LOG_ACT_LOAD("[ActLoad] FAIL: delay[%u] read failed\n", i);
                Reset();
                return false;
            }
        }
    }

    LOG_ACT_LOAD("[ActLoad] SUCCESS: '%s' actions=%u\n", fName ? fName : "(null)", header.count);
    return true;
}

bool CActRes::Load(const char* fName) { return CRes::Load(fName); }
CRes* CActRes::Clone() { return new CActRes(); }
void CActRes::Reset() { actions.clear(); m_events.clear(); m_delay.clear(); numMaxClipPerMotion = 0; }

float CActRes::GetDelay(int actionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(m_delay.size())) {
        return kDefaultMotionDelay;
    }
    const float delay = m_delay[actionIndex];
    return std::isfinite(delay) && delay > 0.0f ? (std::max)(kMinimumMotionDelay, delay) : kDefaultMotionDelay;
}

int CActRes::GetMotionCount(int actionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(actions.size())) {
        return 0;
    }
    return static_cast<int>(actions[actionIndex].motions.size());
}

int CActRes::GetAccessoryMotionIndex(int actionIndex, int headMotionIndex, u32 elapsedMs) const
{
    return GetAccessoryMotionSample(actionIndex, headMotionIndex, elapsedMs).current;
}

ActMotionSample CActRes::GetAccessoryMotionSample(int actionIndex, int headMotionIndex, u32 elapsedMs) const
{
    const int motionCount = GetMotionCount(actionIndex);
    const int bodyBand = actionIndex & ~7;
    // Idle and sitting ACTs contain three head-direction groups. Static
    // equipment has one frame per group; animated equipment loops within
    // the selected group without advancing the body or head pose.
    if ((bodyBand == 0 || bodyBand == 16) && motionCount > 3 && motionCount % 3 == 0) {
        const int framesPerHeadDirection = motionCount / 3;
        const int headDirection = (std::max)(0, (std::min)(headMotionIndex, 2));
        const double progress = static_cast<double>(elapsedMs)
            / (static_cast<double>(GetDelay(actionIndex)) * kActDelayUnitMs);
        return SampleActMotion(progress, framesPerHeadDirection, GetDelay(actionIndex) * kActDelayUnitMs,
            false, headDirection * framesPerHeadDirection);
    }
    return ActMotionSample::Pose(headMotionIndex >= 0 && headMotionIndex < motionCount ? headMotionIndex : 0);
}

const CMotion* CActRes::GetMotion(int actionIndex, int motionIndex) const {
    if (actionIndex < 0 || actionIndex >= static_cast<int>(actions.size())) {
        return nullptr;
    }
    const CAction& action = actions[actionIndex];
    if (motionIndex < 0 || motionIndex >= static_cast<int>(action.motions.size())) {
        return nullptr;
    }
    return &action.motions[motionIndex];
}

const char* CActRes::GetEventName(int eventIndex) const {
    if (eventIndex < 0 || eventIndex >= static_cast<int>(m_events.size())) {
        return "";
    }
    return m_events[static_cast<size_t>(eventIndex)].c_str();
}

// CGameObject stubs
// CGameObject::~CGameObject() {} // defined in header as virtual

// CRenderObject stubs
CRenderObject::CRenderObject()
    : m_pos{ 0.0f, 0.0f, 0.0f }
    , m_baseAction(0)
    , m_curAction(0)
    , m_curMotion(0)
    , m_oldBaseAction(0)
    , m_oldMotion(0)
    , m_bodyPalette(0)
    , m_roty(0.0f)
    , m_zoom(1.0f)
    , m_shadowZoom(1.0f)
    , m_motionSpeed(1.0f)
    , m_lastPixelRatio(1.0f)
    , m_loopCountOfmotionFinish(1.0f)
    , m_modifyFactorOfmotionSpeed(1.0f)
    , m_modifyFactorOfmotionSpeed2(1.0f)
    , m_motionType(0)
    , m_stateId(0)
    , m_oldstateId(0)
    , m_sprShift(0)
    , m_sprAngle(0)
    , m_offsetOow(0)
    , m_colorOfSingleColor(0xFFFF0000u)
    , m_singleColorStartTick(timeGetTime())
    , m_stateStartTick(timeGetTime())
    , m_oldColor{ 0, 0, 0, 0 }
    , m_curColor{ 0, 0, 0, 0 }
    , m_isLieOnGround(0)
    , m_isMotionFinished(0)
    , m_isMotionFreezed(0)
    , m_isSingleColor(0)
    , m_isVisible(1)
    , m_isVisibleBody(1)
    , m_alwaysTopLayer(0)
    , m_isSprArgbFixed(0)
    , m_shadowOn(0)
    , m_shouldAddPickInfo(0)
    , m_isPc(0)
    , m_lastTlvertX(0)
    , m_lastTlvertY(0)
    , m_forceAct(0)
    , m_forceMot(0)
    , m_forceAct2{ 0, 0, 0, 0, 0 }
    , m_forceMot2{ 0, 0, 0, 0, 0 }
    , m_forceMaxMot(0)
    , m_forceAnimSpeed(0)
    , m_forceFinishedAct(0)
    , m_forceFinishedMot(0)
    , m_forceStartMot(0)
    , m_isForceState(0)
    , m_isForceAnimLoop(0)
    , m_isForceAnimation(0)
    , m_isForceAnimFinish(0)
    , m_isForceState2(0)
    , m_isForceState3(0)
    , m_forceStateCnt(0)
    , m_forceStateEndTick(0)
    , m_BodyLight(0)
    , m_BeZero(0)
    , m_BodyFlag(0)
    , m_BodySin(0)
    , m_BodySin2(0)
    , m_BodySin3(0)
    , m_BodySin4(0)
    , m_BodySin5(0)
    , m_BodyTime(0)
    , m_BodyTime2(0)
    , m_BodyTime3(0)
    , m_FlyMove(0)
    , m_FlyNow(0)
    , m_camp(0)
    , m_charfont(0)
    , m_BodyAni(0)
    , m_BodyAct(0)
    , m_BodyAniFrame(0)
    , m_sprRes(nullptr)
    , m_actRes(nullptr)
{
    m_u28.m_sprArgb = 0xFFFFFFFFu;
}

void CRenderObject::SetAction(int act, int mot, int type) {
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    if (CGameActor* actor = dynamic_cast<CGameActor*>(this)) {
        if (actor->m_isPc != 0) {
            if (CActRes* transientActRes = ResolvePcTransientActRes(*actor, act)) {
                actRes = transientActRes;
            }
        }
    }
    const float baseDelay = actRes ? actRes->GetDelay(act) : kDefaultMotionDelay;
    const float clampedDelay = (std::max)(kMinimumMotionDelay, baseDelay);

    m_baseAction = act;
    m_curAction = act;
    m_curMotion = 0;
    m_oldBaseAction = act;
    m_oldMotion = -1;
    m_motionType = type;
    m_stateStartTick = timeGetTime();
    m_motionSpeed = clampedDelay * (m_modifyFactorOfmotionSpeed > 0.0f
        ? m_modifyFactorOfmotionSpeed
        : kDefaultMotionSpeedFactor);
    m_isMotionFinished = 0;
    m_loopCountOfmotionFinish = 1.0f;
    m_modifyFactorOfmotionSpeed = kDefaultMotionSpeedFactor;
    m_isForceState = 0;
    m_isForceState2 = 0;
    m_isForceState3 = 0;

    if (CGameActor* actor = dynamic_cast<CGameActor*>(this)) {
        const bool isLocalPlayer = actor->m_gid != 0
            && (actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid);
        if (kLogBillboardDiagnostics && isLocalPlayer && (act == 24 || act == 80 || act == 88)) {
            DbgLog("[ActorTrace] SetAction gid=%u state=%d act=%d mot=%d type=%d delay=%.3f motionSpeed=%.3f\n",
                actor->m_gid,
                actor->m_stateId,
                act,
                mot,
                type,
                clampedDelay,
                m_motionSpeed);
        }
    }
}

void CRenderObject::ProcessMotion() {
    if (const auto* actor = dynamic_cast<const CGameActor*>(this); actor && actor->IsStatusAnimationFrozen()) {
        return;
    }
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    if (CGameActor* actor = dynamic_cast<CGameActor*>(this)) {
        if (actor->m_isPc != 0) {
            if (CActRes* transientActRes = ResolvePcTransientActRes(*actor, m_baseAction)) {
                actRes = transientActRes;
            }
        }
    }
    if (!actRes) {
        return;
    }

    const CGameActor* actorForMotionWaveTracking = dynamic_cast<CGameActor*>(this);
    const bool trackMotionWaveEventsInProcessMotion = actorForMotionWaveTracking != nullptr;

    if (trackMotionWaveEventsInProcessMotion && m_oldBaseAction != m_baseAction) {
        m_oldMotion = -1;
    }

    const int resolvedAction = m_baseAction + Get8Dir(m_roty);

    const int availableAction = ResolveAvailableActionIndex(actRes, resolvedAction);
    m_curAction = availableAction >= 0 ? availableAction : resolvedAction;

    if (CGameActor* actor = dynamic_cast<CGameActor*>(this)) {
        const bool isLocalPlayer = actor->m_gid != 0
            && (actor->m_gid == g_session.m_gid || actor->m_gid == g_session.m_aid);
        if (kLogMotionProgress && isLocalPlayer && (actor->m_stateId == kAttackStateId
                || actor->m_stateId == kSecondAttackStateId
                || actor->m_stateId == kGameActorPickupStateId)) {
            DbgLog("[ActorTrace] ProcessMotion gid=%u state=%d base=%d dir=%d resolved=%d available=%d cur=%d\n",
                actor->m_gid,
                actor->m_stateId,
                m_baseAction,
                Get8Dir(m_roty),
                resolvedAction,
                availableAction,
                m_curAction);
        }
    }

    const int motionCount = actRes->GetMotionCount(m_curAction);
    if (motionCount <= 0) {
        m_curMotion = 0;
        if (trackMotionWaveEventsInProcessMotion) {
            m_oldBaseAction = m_baseAction;
            m_oldMotion = 0;
        }
        return;
    }

    if (m_isMotionFinished || m_isMotionFreezed) {
        m_curMotion = (std::min)(m_curMotion, motionCount - 1);
        if (trackMotionWaveEventsInProcessMotion) {
            m_oldBaseAction = m_baseAction;
            m_oldMotion = m_curMotion;
        }
        return;
    }

    const int previousMotion = trackMotionWaveEventsInProcessMotion
        ? (std::max)(-1, (std::min)(m_oldMotion, motionCount - 1))
        : 0;

    const float motionProgress = ResolveActorMotionProgress(*this, actRes, m_curAction);

    if (m_motionType == 1) {
        m_curMotion = ResolveMotionFrame(motionProgress, motionCount, true);

        const float loopProgress = motionProgress / static_cast<float>(motionCount);
        if (loopProgress >= 1.0f) {
            m_curMotion = motionCount - 1;
            if (loopProgress >= m_loopCountOfmotionFinish) {
                m_isMotionFinished = 1;
            }
        }
        if (trackMotionWaveEventsInProcessMotion) {
            PlayMotionWaveEvents(this, actRes, m_curAction, previousMotion, m_curMotion);
            m_oldBaseAction = m_baseAction;
            m_oldMotion = m_curMotion;
        }
        return;
    }

    m_curMotion = ResolveMotionFrame(motionProgress, motionCount, false);
    if (trackMotionWaveEventsInProcessMotion) {
        PlayMotionWaveEvents(this, actRes, m_curAction, previousMotion, m_curMotion);
        m_oldBaseAction = m_baseAction;
        m_oldMotion = m_curMotion;
    }
}

void CRenderObject::SetRenderInfo(RENDER_INFO_RECT* rect, float f1, float f2) {
}

void CRenderObject::SetTlvert(float x, float y) {
}

CAbleToMakeEffect::CAbleToMakeEffect()
    : m_efId(-1)
    , m_Sk_Level(0)
    , m_isLoop(0)
    , m_beginSpellEffect(nullptr)
    , m_magicTargetEffect(nullptr)
{
}

CAbleToMakeEffect::~CAbleToMakeEffect()
{
    DetachEffects();
}

CRagEffect* CAbleToMakeEffect::LaunchEffect(int effectId, vector3d deltaPos, float fRot)
{
    if (!g_session.m_isEffectOn) {
        return nullptr;
    }

    // Blood Sport's server timer sends 871 every five seconds. Refresh the
    // existing mist rather than stacking another layer over the status aura.
    if (effectId == kRagEffectBloodsportMist) {
        for (CRagEffect* effect : m_effectList) {
            if (effect && effect->GetEffectType() == effectId) {
                effect->RefreshBloodsportMist();
                return effect;
            }
        }
    }
    const auto* actor = dynamic_cast<const CGameActor*>(this);
    const bool freezeStatusEffect = (effectId == kRagEffectFrozenWater || effectId == kRagEffectFrozenCast
        || effectId == kRagEffectFrostbite)
        && actor && actor->IsFreezeStatusActive();
    if (effectId == kRagEffectMilleniumShields || effectId == kRagEffectEnergyCoatAura
        || (effectId >= kRagEffectSphereEarth && effectId <= kRagEffectSphereWater)
        || freezeStatusEffect) {
        // Status refreshes and frame updates keep one continuous aura.
        for (CRagEffect* effect : m_effectList) {
            if (effect && effect->GetEffectType() == effectId) {
                if (freezeStatusEffect) effect->SetFreezeStatusLoop();
                return effect;
            }
        }
    }

    CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    if (!gameMode || !gameMode->m_world) {
        return nullptr;
    }

    const float sinDeg = std::sin(fRot * (3.14159265f / 180.0f));
    const float cosDeg = std::cos(fRot * (3.14159265f / 180.0f));
    const vector3d rotatedDelta = {
        deltaPos.x * cosDeg - deltaPos.z * sinDeg,
        deltaPos.y,
        deltaPos.z * cosDeg + deltaPos.x * sinDeg
    };

    CRagEffect* effect = new CRagEffect();
    if (!effect) {
        return nullptr;
    }

    effect->Init(this, effectId, rotatedDelta);
    if (freezeStatusEffect) effect->SetFreezeStatusLoop();
    gameMode->m_world->m_gameObjectList.push_back(effect);
    m_effectList.push_back(effect);
    return effect;
}

CRagEffect* CAbleToMakeEffect::LaunchCastingEffect(int effectId, int castTimeMs)
{
    CRagEffect* effect = LaunchEffect(effectId);
    if (effect) {
        effect->SetCastDurationMs(castTimeMs);
        m_castingEffects.push_back(effect);
    }
    return effect;
}

void CAbleToMakeEffect::CancelCastingEffects()
{
    for (CRagEffect* effect : m_castingEffects) {
        if (effect) effect->Cancel();
    }
    m_castingEffects.clear();
}

void CAbleToMakeEffect::RemoveEffectById(int effectId)
{
    if (effectId <= 0) {
        return;
    }

    for (auto it = m_effectList.begin(); it != m_effectList.end();) {
        CRagEffect* effect = *it;
        if (!effect) {
            it = m_effectList.erase(it);
            continue;
        }

        if (effect->GetEffectType() != effectId) {
            ++it;
            continue;
        }

        if (m_beginSpellEffect == effect) {
            m_beginSpellEffect = nullptr;
        }
        if (m_magicTargetEffect == effect) {
            m_magicTargetEffect = nullptr;
        }

        if (effectId == kRagEffectMilleniumShields || effect->IsFreezeStatusLoop()) effect->Cancel();
        effect->SendMsg(effect, 109, 0, 0, 0);
        effect->DetachFromMaster(this);
        it = m_effectList.erase(it);
    }
}

void CAbleToMakeEffect::DetachEffects()
{
    CancelCastingEffects();
    for (CRagEffect* effect : m_effectList) {
        if (effect) {
            if (effect->GetEffectType() == kRagEffectMilleniumShields) effect->Cancel();
            effect->DetachFromMaster(this);
        }
    }
    m_effectList.clear();
    m_beginSpellEffect = nullptr;
    m_magicTargetEffect = nullptr;
}

CGameActor::CGameActor()
    : m_moveDestX(0)
    , m_moveDestY(0)
    , m_moveSrcX(0)
    , m_moveSrcY(0)
    , m_speed(0)
    , m_isCounter(0)
    , m_isTrickDead(0)
    , m_isPlayHitWave(0)
    , m_isAsuraAttack(0)
    , m_emblemWnd(nullptr)
    , m_WordDisplayWnd(nullptr)
    , m_hitWaveName{}
    , m_colorEndTick(0)
    , m_clevel(0)
    , m_MaxHp(0)
    , m_Hp(0)
    , m_MaxSp(0)
    , m_Sp(0)
    , m_Exp(0)
    , m_Str(0)
    , m_Int(0)
    , m_Dex(0)
    , m_Vit(0)
    , m_Luk(0)
    , m_Agi(0)
    , m_accel{ 0.0f, 0.0f, 0.0f }
    , m_moveStartTime(0)
    , m_isNeverAnimation(0)
    , m_pathStartCell(-1)
    , m_dist(0.0f)
    , m_lastProcessStateTime(timeGetTime())
    , m_lastServerTime(g_session.GetServerTime())
    , m_chatTick(0)
    , m_targetGid(0)
    , m_attackMotion(-1.0f)
    , m_isBladeStop(0)
    , m_gid(0)
    , m_job(0)
    , m_sex(0)
    , m_bodyStyle(0)
    , m_balloon(nullptr)
    , m_chatTitle(nullptr)
    , m_merchantShopTitle(nullptr)
    , m_skillRechargeGage(nullptr)
    , m_freezeEndTick(0)
    , m_petEmotionStartTick(0)
    , m_skillRechargeEndTick(timeGetTime() - 5000)
    , m_skillRechargeStartTick(timeGetTime() - 10000)
    , m_chatWidth(0)
    , m_chatHeight(0)
    , m_nameWidth(0)
    , m_xSize(0)
    , m_ySize(0)
    , m_headType(0)
    , m_willBeDead(0)
    , m_is99(0)
    , m_99(0)
    , m_bodyState(0)
    , m_effectState(0)
    , m_healthState(0)
    , m_pkState(0)
    , m_isSitting(0)
    , m_damageDestX(0.0f)
    , m_damageDestZ(0.0f)
    , m_effectLaunchCnt(3600)
    , m_vanishTime(0)
    , m_hideFadeStartTick(0)
    , m_hideFadeEndTick(0)
    , m_actorType(0)
    , m_bIsMemberAndVisible(0)
    , m_gdid(0)
    , m_emblemVersion(0)
    , m_homunAI(nullptr)
    , m_merAI(nullptr)
    , m_objectType(4)
    , m_homunMsg{}
    , m_homunResMsg{}
    , m_merMsg{}
    , m_merResMsg{}
    , m_birdEffect(nullptr)
    , m_cartEffect(nullptr)
    , m_lastDamageNumberEffect(nullptr)
    , m_moveStartPos{ 0.0f, 0.0f, 0.0f }
    , m_moveEndPos{ 0.0f, 0.0f, 0.0f }
    , m_moveEndTime(0)
    , m_isMoving(0)
{
    m_shadowOn = 1;
    m_shouldAddPickInfo = 1;
    m_stateStartTick = timeGetTime();
    m_u28.m_sprArgb = 0xFFFFFFFFu;
    std::memset(m_hitWaveName, 0, sizeof(m_hitWaveName));
    std::memset(&m_homunMsg, 0, sizeof(m_homunMsg));
    std::memset(&m_homunResMsg, 0, sizeof(m_homunResMsg));
    std::memset(&m_merMsg, 0, sizeof(m_merMsg));
    std::memset(&m_merResMsg, 0, sizeof(m_merResMsg));
}

CGameActor::~CGameActor()
{
    DestroySkillRechargeGage();
    DetachEffects();
    const std::vector<CMsgEffect*> ownedMsgEffects(m_msgEffectList.begin(), m_msgEffectList.end());
    m_msgEffectList.clear();
    m_birdEffect = nullptr;
    m_cartEffect = nullptr;
    m_lastDamageNumberEffect = nullptr;
    for (CMsgEffect* effect : ownedMsgEffects) {
        if (!effect) {
            continue;
        }
        if (!IsLiveMsgEffectObject(effect)) {
            continue;
        }
        // Floating numbers finish at their captured position even after a lethal hit removes the actor.
        if (effect->m_msgEffectType == 14 || effect->m_msgEffectType == 16
            || effect->m_msgEffectType == 21 || effect->m_msgEffectType == kTotalDamageNumberKind
            || effect->m_msgEffectType == 114) {
            effect->OnActorDeleted(this);
            continue;
        }
        effect->SendMsg(this, 49, 0, 0, 0);
        effect->SendMsg(this, 53, 0, 0, 0);
    }
}

void CGameActor::SetChatBubbleText(const std::string& text, u32 untilTick)
{
    if (text.empty() || untilTick == 0) {
        ClearChatBubbleText();
        return;
    }

    m_chatBubbleText = text;
    m_chatTick = untilTick;
}

void CGameActor::ClearChatBubbleText()
{
    m_chatBubbleText.clear();
    m_chatTick = 0;
}

bool CGameActor::HasActiveChatBubble(u32 now) const
{
    return !m_chatBubbleText.empty()
        && m_chatTick != 0
        && static_cast<s32>(m_chatTick - now) > 0;
}

void CGameActor::ResetSkillPresentationForCast(int skillId)
{
    m_presentedCastSkillIds.erase(skillId);
    if (m_lastPresentedSkillId == skillId) m_lastPresentedSkillId = 0;
}

bool CGameActor::BeginSkillPresentation(int skillId, const std::string& name, u32 now, bool oncePerCast)
{
    // Area skills send one damage packet per victim. Keep the caster's action and shout continuous.
    if (m_lastPresentedSkillId == skillId && now - m_lastSkillPresentationTick < 120) {
        return false;
    }
    if (oncePerCast) {
        // Older impacts must not finish a new cast of the same skill early.
        if (m_castingSkillId == skillId && static_cast<s32>(m_skillRechargeEndTick - now) > 0) {
            return false;
        }
        if (!m_presentedCastSkillIds.insert(skillId).second) return false;
    }
    m_lastPresentedSkillId = skillId;
    m_lastSkillPresentationTick = now;
    // Rend is an automatic attack: keep its presentation without creating or
    // refreshing a skill-name shout over the actor.
    if (skillId != kSkillIdRend && !name.empty()) {
        m_skillNameText = name;
        while (!m_skillNameText.empty() && m_skillNameText.back() == '!') {
            m_skillNameText.pop_back();
        }
        m_skillNameText += "!!";
        m_skillNameUntilTick = now + 1500;
    }
    return true;
}

bool CGameActor::HasActiveSkillName(u32 now) const
{
    return !m_skillNameText.empty() && static_cast<s32>(m_skillNameUntilTick - now) > 0;
}

bool CGameActor::IsFreezeStatusActive() const
{
    return m_bodyState == kActorBodyStateFreeze && m_stateId != kDeathStateId;
}

bool CGameActor::IsStatusAnimationFrozen() const
{
    return m_stateId != kDeathStateId
        && (m_bodyState == kActorBodyStateStone || m_bodyState == kActorBodyStateFreeze);
}

u32 CGameActor::GetBodyStateSpriteTint() const
{
    if (m_stateId == kDeathStateId) return 0xFFFFFFu;
    switch (m_bodyState) {
    case kActorBodyStateStoneWait: return 0xE6CDAAu; // Mild brown while petrifying.
    case kActorBodyStateStone: return 0xA06F46u; // Strong brown while petrified.
    case kActorBodyStateFreeze: return 0x80C0FFu; // Blue while frozen.
    default: return 0xFFFFFFu;
    }
}

void CGameActor::StartHideFade(u32 durationMs)
{
    const u32 now = timeGetTime();
    const u32 clampedDuration = (std::max)(1u, durationMs);
    m_hideFadeStartTick = now;
    m_hideFadeEndTick = now + clampedDuration;
    m_isVisible = 1;
    m_isVisibleBody = 1;
    m_shadowOn = 0;
}

void CGameActor::CancelHideFade()
{
    m_hideFadeStartTick = 0;
    m_hideFadeEndTick = 0;
}

bool CGameActor::IsHideFadeActive(u32 now) const
{
    return m_hideFadeEndTick != 0 && now < m_hideFadeEndTick;
}

u32 CGameActor::GetHideFadeAlpha(u32 now) const
{
    if (!IsHideFadeActive(now) || m_hideFadeEndTick <= m_hideFadeStartTick) {
        return 255u;
    }

    const u32 total = m_hideFadeEndTick - m_hideFadeStartTick;
    const u32 remaining = m_hideFadeEndTick - now;
    return (std::min)(255u, (remaining * 255u) / total);
}

void CGameActor::SetFrenzyStatus(bool active, u32 remainingMs)
{
    m_frenzyStatusActive = active;
    m_frenzyStatusStartTick = timeGetTime();
    // Legacy 0196 has no timer; an infinite duration also waits for removal.
    m_frenzyStatusDurationMs = active && remainingMs != 0xFFFFFFFFu ? remainingMs : 0;
}

bool CGameActor::IsFrenzyStatusActive(u32 now) const
{
    return m_frenzyStatusActive && m_stateId != kDeathStateId
        && (m_frenzyStatusDurationMs == 0 || now - m_frenzyStatusStartTick < m_frenzyStatusDurationMs);
}

void CGameActor::SetBloodsportStatus(bool active, u32 remainingMs)
{
    m_bloodsportStatusActive = active;
    m_bloodsportStatusStartTick = timeGetTime();
    // Legacy 0196 and infinite timers wait for the removal notification.
    m_bloodsportStatusDurationMs = active && remainingMs != 0xFFFFFFFFu ? remainingMs : 0;
    if (IsBloodsportStatusActive(m_bloodsportStatusStartTick)) {
        LaunchEffect(kRagEffectBloodsportMist);
    } else {
        RemoveEffectById(kRagEffectBloodsportMist);
    }
}

bool CGameActor::IsBloodsportStatusActive(u32 now) const
{
    return m_bloodsportStatusActive && m_stateId != kDeathStateId
        && (m_bloodsportStatusDurationMs == 0
            || now - m_bloodsportStatusStartTick < m_bloodsportStatusDurationMs);
}

void CGameActor::SetMilleniumShieldsStatus(bool active, u32 remainingMs)
{
    m_milleniumShieldsStatusActive = active;
    m_milleniumShieldsStatusStartTick = timeGetTime();
    // Legacy 0196 and infinite timers wait for the removal notification.
    m_milleniumShieldsStatusDurationMs = active && remainingMs != 0xFFFFFFFFu ? remainingMs : 0;
    if (IsMilleniumShieldsStatusActive(m_milleniumShieldsStatusStartTick)) {
        LaunchEffect(kRagEffectMilleniumShields);
    } else {
        RemoveEffectById(kRagEffectMilleniumShields);
    }
}

bool CGameActor::IsMilleniumShieldsStatusActive(u32 now) const
{
    return m_milleniumShieldsStatusActive && m_stateId != kDeathStateId
        && (m_milleniumShieldsStatusDurationMs == 0
            || now - m_milleniumShieldsStatusStartTick < m_milleniumShieldsStatusDurationMs);
}

void CGameActor::SetEnergyCoatStatus(bool active, u32 remainingMs)
{
    m_energyCoatStatusActive = active;
    m_energyCoatStatusStartTick = timeGetTime();
    // Legacy 0196 and infinite timers wait for the removal notification.
    m_energyCoatStatusDurationMs = active && remainingMs != 0xFFFFFFFFu ? remainingMs : 0;
    if (IsEnergyCoatStatusActive(m_energyCoatStatusStartTick)) {
        LaunchEffect(kRagEffectEnergyCoatAura);
    } else {
        RemoveEffectById(kRagEffectEnergyCoatAura);
    }
}

bool CGameActor::IsEnergyCoatStatusActive(u32 now) const
{
    return m_energyCoatStatusActive && m_stateId != kDeathStateId
        && (m_energyCoatStatusDurationMs == 0
            || now - m_energyCoatStatusStartTick < m_energyCoatStatusDurationMs);
}

void CGameActor::SetSphereStatus(int sphereIndex, bool active)
{
    if (sphereIndex < 0 || sphereIndex >= static_cast<int>(m_sphereStatusActive.size())) return;
    m_sphereStatusActive[sphereIndex] = active && m_stateId != kDeathStateId;
    const int effectId = kRagEffectSphereEarth + sphereIndex;
    if (IsSphereStatusActive(sphereIndex)) {
        LaunchEffect(effectId);
    } else {
        RemoveEffectById(effectId);
    }
}

bool CGameActor::IsSphereStatusActive(int sphereIndex) const
{
    return sphereIndex >= 0 && sphereIndex < static_cast<int>(m_sphereStatusActive.size())
        && m_sphereStatusActive[sphereIndex] && m_stateId != kDeathStateId;
}

void CGameActor::ProcessStatusEffects()
{
    for (int sphereIndex = 0; sphereIndex < static_cast<int>(m_sphereStatusActive.size()); ++sphereIndex) {
        if (!m_sphereStatusActive[sphereIndex]) continue;
        if (!IsSphereStatusActive(sphereIndex)) {
            SetSphereStatus(sphereIndex, false);
        } else if (g_session.m_isEffectOn) {
            LaunchEffect(kRagEffectSphereEarth + sphereIndex);
        } else {
            RemoveEffectById(kRagEffectSphereEarth + sphereIndex);
        }
    }

    if (IsFreezeStatusActive() && g_session.m_isEffectOn) {
        // Adopt existing Frost Diver impacts as loops without restarting them.
        LaunchEffect(kRagEffectFrozenWater);
        LaunchEffect(kRagEffectFrozenCast);
        LaunchEffect(kRagEffectFrostbite);
    } else {
        // End only the status loops; ordinary skill impacts still finish once.
        for (auto it = m_effectList.begin(); it != m_effectList.end();) {
            CRagEffect* effect = *it;
            if (effect && effect->IsFreezeStatusLoop()) {
                effect->Cancel();
                effect->DetachFromMaster(this);
                it = m_effectList.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (m_energyCoatStatusActive) {
        if (!IsEnergyCoatStatusActive(timeGetTime())) {
            SetEnergyCoatStatus(false, 0);
        } else if (g_session.m_isEffectOn) {
            LaunchEffect(kRagEffectEnergyCoatAura);
        } else {
            RemoveEffectById(kRagEffectEnergyCoatAura);
        }
    }

    if (m_milleniumShieldsStatusActive) {
        if (!IsMilleniumShieldsStatusActive(timeGetTime())) {
            SetMilleniumShieldsStatus(false, 0);
        } else if (g_session.m_isEffectOn) {
            // Restores the loop if effects were disabled when the status started.
            LaunchEffect(kRagEffectMilleniumShields);
        } else {
            RemoveEffectById(kRagEffectMilleniumShields);
        }
    }

    if (m_bloodsportStatusActive) {
        if (IsBloodsportStatusActive(timeGetTime())) {
            // Also restores the aura if effects were disabled when it started.
            LaunchEffect(kRagEffectBloodsportMist);
        } else {
            SetBloodsportStatus(false, 0);
        }
    }

    if (m_effectState == 0) {
        m_effectLaunchCnt = 36000;
    } else {
        if ((m_effectState & kEffectStateRuwachMask) != 0 && m_effectLaunchCnt >= kRuwachRelaunchTicks) {
            m_effectLaunchCnt = 0;
            if (CRagEffect* effect = LaunchEffect(33, vector3d{}, 0.0f)) {
                effect->SendMsg(effect, 44, (timeGetTime() >> 4) % 10 + 1, 0, 0);
            }
        }

        ++m_effectLaunchCnt;
    }
}

u8 CGameActor::ProcessState() {
    ProcessWillBeAttacked();

    if (m_hideFadeEndTick != 0 && timeGetTime() >= m_hideFadeEndTick) {
        const bool hiding = (m_effectState & kEffectStateHidingMask) != 0;
        const bool specialHiding = (m_effectState & kEffectStateSpecialHidingMask) != 0;
        const bool burrow = (m_effectState & kEffectStateBurrowMask) != 0;
        CancelHideFade();
        m_isVisibleBody = (!(hiding || specialHiding || burrow)) ? 1 : 0;
        m_isVisible = (!(hiding || specialHiding)) ? 1 : 0;
        m_shadowOn = (!(hiding || specialHiding || burrow)) ? 1 : 0;
    }

    if (m_isMoving) {
        const u32 now = g_session.GetServerTime();
        const u32 wallNow = timeGetTime();
        const vector3d prevPos = m_pos;
        if (m_path.m_cells.size() >= 2 && InterpolatePathPosition(*this, now, &m_pos)) {
            if (now >= m_moveEndTime) {
                m_pos = m_moveEndPos;
                m_isMoving = 0;
            }
        } else if (m_moveEndTime <= m_moveStartTime || now >= m_moveEndTime) {
            m_pos = m_moveEndPos;
            m_isMoving = 0;
        } else {
            const float duration = static_cast<float>(m_moveEndTime - m_moveStartTime);
            const float ratio = static_cast<float>(now - m_moveStartTime) / duration;
            const float clamped = (std::max)(0.0f, (std::min)(1.0f, ratio));
            m_pos.x = m_moveStartPos.x + (m_moveEndPos.x - m_moveStartPos.x) * clamped;
            m_pos.y = m_moveStartPos.y + (m_moveEndPos.y - m_moveStartPos.y) * clamped;
            m_pos.z = m_moveStartPos.z + (m_moveEndPos.z - m_moveStartPos.z) * clamped;
        }

        if (prevPos.x != m_pos.x || prevPos.z != m_pos.z) {
            const float dx = m_pos.x - prevPos.x;
            const float dz = m_pos.z - prevPos.z;
            m_dist += std::sqrt(dx * dx + dz * dz);
            const float rotation = std::atan2(m_pos.x - prevPos.x, -(m_pos.z - prevPos.z)) * (180.0f / 3.14159265f);
            m_roty = NormalizeAngle360(rotation);
        }

        if (!m_isNeverAnimation) {
            m_curAction = m_baseAction + Get8Dir(m_roty);
        }

        if (m_gid != 0 && m_isPc == 0 && m_stateId != kDeathStateId) {
            MoveStallTraceState& trace = g_moveStallTraceByGid[m_gid];
            const bool movedThisFrame = prevPos.x != m_pos.x || prevPos.z != m_pos.z;
            if (movedThisFrame) {
                if (trace.wasStalled) {
                    DbgLog("[ActorMove] resume gid=%u pos=(%.2f,%.2f)->(%.2f,%.2f) serverNow=%u move=[%u..%u] pathCells=%zu\n",
                        m_gid,
                        prevPos.x,
                        prevPos.z,
                        m_pos.x,
                        m_pos.z,
                        static_cast<unsigned int>(now),
                        static_cast<unsigned int>(m_moveStartTime),
                        static_cast<unsigned int>(m_moveEndTime),
                        m_path.m_cells.size());
                }
                trace.lastPosX = m_pos.x;
                trace.lastPosZ = m_pos.z;
                trace.lastMoveWallTick = wallNow;
                trace.lastLogWallTick = 0;
                trace.wasStalled = false;
            } else {
                if (trace.lastMoveWallTick == 0) {
                    trace.lastPosX = m_pos.x;
                    trace.lastPosZ = m_pos.z;
                    trace.lastMoveWallTick = wallNow;
                }
                const u32 stalledForMs = wallNow - trace.lastMoveWallTick;
                const bool moveShouldStillAdvance = m_moveEndTime > m_moveStartTime && now < m_moveEndTime;
                if (moveShouldStillAdvance
                    && stalledForMs >= kRemoteMoveStallLogThresholdMs
                    && (trace.lastLogWallTick == 0 || wallNow - trace.lastLogWallTick >= kRemoteMoveStallLogThresholdMs)) {
                    DbgLog("[ActorMove] stall gid=%u pos=(%.2f,%.2f) serverNow=%u stalledFor=%u move=[%u..%u] remaining=%u pathCells=%zu state=%d\n",
                        m_gid,
                        m_pos.x,
                        m_pos.z,
                        static_cast<unsigned int>(now),
                        static_cast<unsigned int>(stalledForMs),
                        static_cast<unsigned int>(m_moveStartTime),
                        static_cast<unsigned int>(m_moveEndTime),
                        static_cast<unsigned int>(m_moveEndTime - now),
                        m_path.m_cells.size(),
                        m_stateId);
                    trace.lastLogWallTick = wallNow;
                    trace.wasStalled = true;
                }
            }
        }
    }
    else if (m_gid != 0 && m_isPc == 0) {
        g_moveStallTraceByGid.erase(m_gid);
    }

    switch (m_stateId) {
    case 0:
        // Player idle/sit motions select head poses, rather than animation frames.
        if (!m_isPc || m_baseAction == 32) {
            ProcessMotion();
        }
        break;
    case kMoveStateId:
        if (!m_isMoving) {
            SetState(0);
        } else {
            ProcessMotion();
        }
        break;
    case kGameActorPickupStateId:
        m_isCounter = 0;
        m_isMotionFreezed = 0;
        ProcessMotion();
        if (m_isMotionFinished) {
            SetState(0);
        }
        break;
    case kAttackStateId:
    case kSecondAttackStateId: {
        if (m_freezeEndTick != 0) {
            if (timeGetTime() >= m_freezeEndTick) {
                m_freezeEndTick = 0;
                m_isMotionFreezed = 0;
            } else {
                m_isMotionFreezed = 1;
            }
        } else {
            m_isMotionFreezed = 0;
        }
        ProcessMotion();
        if (m_isMotionFinished && m_freezeEndTick == 0) {
            SetState(0);
        }
        break;
    }
    case kGameActorSkillStateId:
    case kGameActorDanceStateId:
    case kGameActorSingStateId:
        m_isCounter = 0;
        m_isMotionFreezed = 0;
        ProcessMotion();
        if (m_isMotionFinished && (m_skillMotionPoseDurationMs == 0
                || timeGetTime() - m_stateStartTick >= static_cast<u32>(m_skillMotionPoseDurationMs))) {
            SetState(0);
        }
        break;
    case kGameActorCastingStateId:
        m_isCounter = 0;
        m_isMotionFreezed = 0;
        ProcessMotion();
        if (m_targetGid != 0) {
            const u32 nCx = timeGetTime() - m_stateStartTick;
            const int slice = static_cast<int>(static_cast<double>(nCx) * (1.0 / 24.0));
            if (slice % 34 == 0) {
                if (CGameActor* target = ResolveActorByGidForCastRetarget(m_targetGid)) {
                    const float dx = target->m_pos.x - m_pos.x;
                    const float dz = target->m_pos.z - m_pos.z;
                    // Self-targeted casts have no facing direction to resolve.
                    if (dx != 0.0f || dz != 0.0f) {
                        m_roty = std::atan2(dx, -dz) * (180.0f / 3.14159265f);
                        if (m_roty >= 360.0f) {
                            m_roty -= 360.0f;
                        }
                        if (m_roty < 0.0f) {
                            m_roty += 360.0f;
                        }
                    }
                }
            }
        }
        if (m_skillRechargeEndTick != 0 && timeGetTime() > m_skillRechargeEndTick) {
            m_skillRechargeStartTick = 0;
            m_skillRechargeEndTick = 0;
            SendMsg(this, 83, 0, 0, 0);
            SetState(0);
        }
        break;
    case kGameActorCastingLoopStateId:
        if (m_skillRechargeEndTick != 0 && timeGetTime() >= m_skillRechargeEndTick) {
            m_isCounter = 0;
            m_isMotionFreezed = 0;
            SetState(0);
            SendMsg(this, 83, 0, 0, 0);
        }
        ProcessMotion();
        break;
    case kDeathStateId:
        m_isMotionFreezed = 0;
        m_isForceState = 0;
        m_isForceState2 = 0;
        m_isForceState3 = 0;
        ProcessMotion();
        LogDeathMotionProgress(*this);
        break;
    case kHitReactionStateId: {
        m_isMotionFreezed = 0;
        ProcessMotion();

        const u32 elapsedMs = timeGetTime() - m_stateStartTick;
        const int frames = m_actRes ? (std::max)(1, m_actRes->GetMotionCount(m_curAction)) : 1;
        const float reactionDurationMs = (std::max)(1.0f,
            static_cast<float>(frames) * m_motionSpeed * kActDelayUnitMs);
        const float ratio = (std::min)(1.0f, static_cast<float>(elapsedMs) / reactionDurationMs);
        m_pos.x = m_moveStartPos.x + (m_moveEndPos.x - m_moveStartPos.x) * ratio;
        m_pos.z = m_moveStartPos.z + (m_moveEndPos.z - m_moveStartPos.z) * ratio;

        if (m_isMotionFinished || static_cast<float>(elapsedMs) >= reactionDurationMs) {
            m_pos.x = m_moveEndPos.x;
            m_pos.z = m_moveEndPos.z;
            m_targetGid = 0;
            m_attackMotion = -1.0f;
            if (m_willBeDead != 0 && m_willBeAttackedList.empty()) {
                SetState(kDeathStateId);
            } else {
                SetState(0);
            }
        }
        break;
    }
    default:
        break;
    }

    ProcessQueuedHitWave(*this);

    return 1;
}

void CGameActor::SendMsg(CGameObject* src, int msg, msgparam_t par1, msgparam_t par2, msgparam_t par3)
{
    switch (msg) {
    case 28:
        if (m_stateId != kDeathStateId
            && m_isPc == 0
            && (m_stateId == kHitReactionStateId || !m_willBeAttackedList.empty())) {
            m_willBeDead = 1;
            return;
        }
        SetState(kDeathStateId);
        return;
    case 109:
        RemoveEffectById(static_cast<int>(par1));
        return;
    case 81:
        m_targetGid = static_cast<u32>(par1);
        return;
    case 82: {
        const u32 now = timeGetTime();
        m_skillRechargeStartTick = now;
        m_skillRechargeEndTick = now + static_cast<u32>((std::max)(0, static_cast<int>(par1)));
        if (!m_skillRechargeGage) {
            auto* gage = new UIRechargeGage();
            if (gage) {
                gage->Create(uitheme::kActorVitalsBarWidth + uitheme::kActorVitalsBorderThickness * 2,
                    uitheme::kActorVitalsBarHeight + uitheme::kActorVitalsBorderThickness * 2);
                g_windowMgr.AddWindowFront(gage);
                m_skillRechargeGage = gage;
            }
        }
        return;
    }
    case 83:
        m_castingSkillId = 0;
        DestroySkillRechargeGage();
        return;
    case 34: {
        CPc* pcActor = dynamic_cast<CPc*>(this);
        if (!pcActor) {
            return;
        }

        if (!pcActor->m_gage) {
            auto* gage = new UIPcGage();
            if (gage) {
                gage->Create(60, 5);
                g_windowMgr.AddWindowFront(gage);
                pcActor->m_gage = gage;
            }
        }

        if (pcActor->m_gage) {
            pcActor->m_gage->SetMode(0);
            pcActor->m_gage->SetHp(static_cast<int>(par1), static_cast<int>(par2));
            pcActor->m_gage->SetSp(0, 0);
        }

        m_Hp = static_cast<u16>((std::max)(0, static_cast<int>(par1)));
        m_MaxHp = static_cast<u16>((std::max)(0, static_cast<int>(par2)));
        return;
    }
    case 35: {
        CPc* pcActor = dynamic_cast<CPc*>(this);
        if (pcActor && pcActor->m_gage) {
            g_windowMgr.DeleteWindow(pcActor->m_gage);
            pcActor->m_gage = nullptr;
        }
        return;
    }
    case 85: {
        const int effectId = static_cast<int>(par1);
        const bool forceRestartBeginEffect = effectId == 123;
        if (effectId <= 0) {
            if (m_beginSpellEffect) {
                m_beginSpellEffect->DetachFromMaster(this);
                m_beginSpellEffect = nullptr;
            }
            return;
        }
        if (m_beginSpellEffect
            && std::find(m_effectList.begin(), m_effectList.end(), m_beginSpellEffect) == m_effectList.end()) {
            m_beginSpellEffect = nullptr;
        }
        // Same begin-spell / buff ring STR as already showing: only refresh duration (msg 80). Re-launching
        // would run CRagEffect::Init again and replay EzStr startup SFX (e.g. Angelus after level-up refresh).
        if (m_beginSpellEffect && m_beginSpellEffect->GetEffectType() == effectId) {
            if (!forceRestartBeginEffect) {
                if (static_cast<int>(par2) > 0) {
                    m_beginSpellEffect->SendMsg(m_beginSpellEffect, 80, par2, 0, 0);
                }
                return;
            }

            RemoveEffectById(effectId);
        }
        if (m_beginSpellEffect) {
            m_beginSpellEffect->DetachFromMaster(this);
            m_beginSpellEffect = nullptr;
        }
        m_beginSpellEffect = LaunchEffect(effectId, vector3d{}, 0.0f);
        if (m_beginSpellEffect) {
            if (static_cast<int>(par2) > 0) {
                m_beginSpellEffect->SendMsg(m_beginSpellEffect, 80, par2, 0, 0);
            }
        }
        return;
    }
    case 86:
        if (!m_magicTargetEffect && static_cast<int>(par1) > 0) {
            m_magicTargetEffect = LaunchEffect(static_cast<int>(par1), vector3d{}, 0.0f);
        }
        if (m_magicTargetEffect) {
            if (static_cast<int>(par2) > 0) {
                m_magicTargetEffect->SendMsg(m_magicTargetEffect, 80, par2, 0, 0);
            }
        }
        return;
    case 87:
        if (m_magicTargetEffect) {
            m_magicTargetEffect->DetachFromMaster(this);
            m_magicTargetEffect = nullptr;
        }
        return;
    case 98:
        m_willBeDead = 0;
        m_vanishTime = 0;
        m_isLieOnGround = 0;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        m_attackMotion = -1.0f;
        m_shouldAddPickInfo = 1;
        m_stateId = 0;
        SetState(0);
        if (CPc* pcActor = dynamic_cast<CPc*>(this)) {
            pcActor->InvalidateBillboard();
        }
        return;
    case 88: {
        CGameActor* sourceActor = dynamic_cast<CGameActor*>(src);
        QueueDamageNumber(sourceActor, static_cast<int>(par1), static_cast<u32>(par2), static_cast<int>(par3));
        return;
    }
    case 89:
        SpawnMissEffect(*this);
        return;
    case 92:
        SpawnCriticalEffect(*this);
        return;
    case 95:
        SpawnLuckyDodgeEffect(*this);
        return;
    case 51:
        SpawnCartEffect(*this, static_cast<int>(par1));
        return;
    case 52:
        DestroyAttachedMsgEffect(*this, &m_cartEffect);
        return;
    default:
        return;
    }
}

void CGameActor::SetState(int state) {
    if (state == kMoveStateId && m_stateId == kMoveStateId && m_baseAction == 8 && m_isMoving) {
        // A path extension continues the current stride and sound event cursor.
        return;
    }
    if (m_stateId == kHitReactionStateId) {
        m_pos.x = m_damageDestX;
        m_pos.z = m_damageDestZ;
    }

    if (state == kHitReactionStateId && (m_isForceState || m_isForceState2 || m_isForceState3)) {
        return;
    }

    switch (state) {
    case 0:
        m_attackMotion = -1.0f;
        m_isMotionFreezed = 0;
        SetAction(0, 4, 0);
        m_isMotionFinished = m_isPc != 0;
        break;
    case kMoveStateId:
        m_attackMotion = -1.0f;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        if (!m_isAsuraAttack) {
            SetAction(8, 6, 0);
        }
        break;
    case kGameActorSkillStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        SetAction(m_isPc != 0 ? 96 : 16, 3, 1);
        m_attackMotion = static_cast<float>(GetAttackMotion());
        break;
    case kGameActorDanceStateId:
    case kGameActorSingStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        m_attackMotion = -1.0f;
        SetAction(m_isPc != 0 ? 96 : 16, 3, 1);
        break;
    case kGameActorPickupStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        m_isCounter = 0;
        m_attackMotion = -1.0f;
        SetAction(24, 6, 1);
        break;
    case kGameActorAttackStateId:
    case kSecondAttackStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        SetAction(state == kSecondAttackStateId
                ? 88
                : (m_isPc != 0 ? ResolvePcAttackAction(*this) : 16),
            3,
            1);
        m_attackMotion = static_cast<float>(GetAttackMotion());
        break;
    case kGameActorCastingStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        m_attackMotion = -1.0f;
        SetAction(m_isPc != 0 ? 72 : 16, 4, 1);
        break;
    case kGameActorCastingLoopStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_isMotionFinished = 0;
        m_isCounter = 0;
        if (m_isPc != 0) {
            SetAction(72, 4, 1);
        } else {
            SetAction(16, 4, 1);
        }
        m_isMotionFreezed = 1;
        break;
    case kHitReactionStateId:
        m_isMoving = 0;
        m_path.Reset();
        m_moveStartPos = m_pos;
        m_moveEndPos = m_pos;
        m_moveEndPos.x = m_damageDestX;
        m_moveEndPos.z = m_damageDestZ;
        m_targetGid = 0;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        SetAction(m_isPc != 0 ? kHitReactionAction : kNonPcHitReactionAction, kHitReactionMotion, 1);
        break;
    case kDeathStateId:
        m_isMoving = 0;
        CancelCastingEffects();
        SetFrenzyStatus(false, 0);
        SetBloodsportStatus(false, 0);
        SetMilleniumShieldsStatus(false, 0);
        SetEnergyCoatStatus(false, 0);
        for (int sphereIndex = 0; sphereIndex < static_cast<int>(m_sphereStatusActive.size()); ++sphereIndex) {
            SetSphereStatus(sphereIndex, false);
        }
        m_path.Reset();
        m_moveStartPos = m_pos;
        m_moveEndPos = m_pos;
        m_moveStartTime = 0;
        m_moveEndTime = 0;
        m_targetGid = 0;
        m_attackMotion = -1.0f;
        m_isLieOnGround = 1;
        m_isMotionFinished = 0;
        m_isMotionFreezed = 0;
        SetAction(m_isPc != 0 ? kDeathAction : kNonPcDeathAction, kDeathMotion, 1);
        break;
    default:
        return;
    }

    m_skillMotionPoseDurationMs = 0;
    m_curAction = m_baseAction + Get8Dir(m_roty);
    if (state == kHitReactionStateId) {
        const int frames = m_actRes ? (std::max)(1, m_actRes->GetMotionCount(m_curAction)) : 1;
        m_motionSpeed = static_cast<float>(kHitReactionDurationMs) / (static_cast<float>(frames) * kActDelayUnitMs);
    }
    if (state == kAttackStateId || state == kSecondAttackStateId) {
        // Apply the same server timing to players and monsters. Use the ACT
        // impact marker from the selected direction/weapon, including fast ASPD.
        m_attackMotion = static_cast<float>(GetAttackMotion());
        if (m_modifyFactorOfmotionSpeed2 > 0.0f) {
            CActRes* actRes = ResolveRuntimeActorActRes(this);
            if (m_isPc) {
                if (CActRes* transientActRes = ResolvePcTransientActRes(*this, m_curAction)) {
                    actRes = transientActRes;
                }
            }
            const int attackAction = actRes ? ResolveAvailableActionIndex(actRes, m_curAction) : m_curAction;
            const int motionCount = actRes ? actRes->GetMotionCount(attackAction) : 1;
            const float timingFrames = m_attackMotion > 0.0f
                ? m_attackMotion : static_cast<float>((std::max)(1, motionCount));
            const float attackTimeMs = m_modifyFactorOfmotionSpeed2 / kAttackMotionFactor;
            m_motionSpeed = (std::max)(kMinimumMotionDelay,
                attackTimeMs / (timingFrames * kActDelayUnitMs));
        }
    }
    if (m_stateId == 0 || m_stateId == 6) {
        m_oldstateId = m_stateId;
    }
    m_stateId = state;
    m_stateStartTick = timeGetTime();

    const bool isLocalPlayer = m_gid != 0 && (m_gid == g_session.m_gid || m_gid == g_session.m_aid);
    if (kLogBillboardDiagnostics && isLocalPlayer && (state == kAttackStateId || state == kSecondAttackStateId || state == kGameActorPickupStateId)) {
        DbgLog("[ActorTrace] SetState gid=%u state=%d base=%d cur=%d motion=%d roty=%.1f dir=%d atkMotion=%.1f weapon=%d shield=%d isPc=%d\n",
            m_gid,
            state,
            m_baseAction,
            m_curAction,
            m_curMotion,
            m_roty,
            Get8Dir(m_roty),
            m_attackMotion,
            dynamic_cast<CPc*>(this) ? dynamic_cast<CPc*>(this)->m_weapon : -1,
            dynamic_cast<CPc*>(this) ? dynamic_cast<CPc*>(this)->m_shield : -1,
            m_isPc);
    }
}

void CGameActor::PlaySkillMotion(int motionType, int durationMs, bool casting)
{
    if (m_stateId == kDeathStateId || m_isTrickDead) {
        return;
    }
    if (motionType == 0 && !casting) {
        SetState(0);
        return;
    }

    SetModifyFactorOfmotionSpeed(durationMs);
    // Presentation motion 1 is skill invocation, not actor state 1 (walking).
    // Use the same job-specific skill action as legacy successful skill use.
    const int motionState = motionType == 1 ? kGameActorSkillStateId : motionType;
    SetState(motionState);
    if (motionType == 0) {
        SetAction(0, 4, 1);
    }

    // The cast's lifetime is independent of its visual. Successful use, cancel,
    // and expiry use the existing casting cleanup.
    if (casting) {
        m_stateId = kGameActorCastingStateId;
    } else if (motionState == kGameActorCastingStateId) {
        m_stateId = kGameActorSkillStateId;
    }
    m_isMoving = 0;
    m_path.Reset();
    m_isCounter = 0;
    // Some jobs' later skill frames are attack poses. Motion 1 holds invocation
    // at both stages; motion 7 remains available for the full skill animation.
    m_isMotionFinished = motionType == 1;
    if (!casting && motionType == 1) {
        m_skillMotionPoseDurationMs = (std::max)(1, durationMs);
    }
    m_isMotionFreezed = 0;
    m_freezeEndTick = 0;
    m_motionType = 1;
    m_curAction = m_baseAction + Get8Dir(m_roty);
    if (motionType == 0 && m_isPc) {
        // Player idle frames are head poses, not an animation to play through.
        m_isMotionFinished = 1;
        const CPc* pcActor = dynamic_cast<const CPc*>(this);
        m_curMotion = pcActor ? pcActor->m_headDir : 0;
    }

    const bool attackMotion = m_stateId == kAttackStateId || m_stateId == kSecondAttackStateId;
    if (!attackMotion) {
        m_attackMotion = -1.0f;
    }
    if (casting || !attackMotion) {
        CActRes* actRes = ResolveRuntimeActorActRes(this);
        if (m_isPc) {
            if (CActRes* bodyActRes = ResolvePcTransientActRes(*this, m_curAction)) {
                actRes = bodyActRes;
            }
        }
        const int action = actRes ? ResolveAvailableActionIndex(actRes, m_curAction) : m_curAction;
        const int frames = actRes ? (std::max)(1, actRes->GetMotionCount(action)) : 1;
        m_motionSpeed = (std::max)(kMinimumMotionDelay,
            static_cast<float>((std::max)(1, durationMs)) / (kActDelayUnitMs * frames));
    }
    ProcessMotion();
    if (CPc* pcActor = dynamic_cast<CPc*>(this)) {
        pcActor->InvalidateBillboard();
    }
}

void CGameActor::DestroySkillRechargeGage()
{
    CancelCastingEffects();
    if (m_skillRechargeGage) {
        g_windowMgr.DeleteWindow(m_skillRechargeGage);
        m_skillRechargeGage = nullptr;
    }
    m_skillRechargeStartTick = 0;
    m_skillRechargeEndTick = 0;
}

void CGameActor::ProcessSkillRechargeGageOverlay(int screenCenterX, int screenTopY)
{
    if (!m_skillRechargeGage || m_skillRechargeEndTick == 0 || m_skillRechargeStartTick == 0) {
        return;
    }

    const u32 now = timeGetTime();
    const int total = static_cast<int>(m_skillRechargeEndTick - m_skillRechargeStartTick);
    if (total <= 0) {
        return;
    }

    if (now <= m_skillRechargeEndTick) {
        const int barX = screenCenterX - m_skillRechargeGage->m_w / 2;
        const int barY = screenTopY - m_skillRechargeGage->m_h - kCastingBarActorGapPx;
        m_skillRechargeGage->Move(barX, barY);
        int elapsed = static_cast<int>(now - m_skillRechargeStartTick);
        if (elapsed < 0) {
            elapsed = 0;
        }
        if (elapsed > total) {
            elapsed = total;
        }
        m_skillRechargeGage->SetAmount(elapsed, total);
    } else {
        SendMsg(this, 83, 0, 0, 0);
        if (m_stateId == kGameActorCastingStateId) {
            SetState(0);
        }
    }
}

void CGameActor::ProcessWillBeAttacked()
{
    const u32 now = timeGetTime();
    if (m_hasNextWhiteDamageNumberTick && static_cast<s32>(now - m_nextWhiteDamageNumberTick) >= 0) {
        m_hasNextWhiteDamageNumberTick = false;
    }
    for (auto it = m_willBeAttackedList.begin(); it != m_willBeAttackedList.end(); ) {
        if (now < it->time) {
            ++it;
            continue;
        }

        ApplyQueuedHitReaction(*this, *it);
        it = m_willBeAttackedList.erase(it);
    }
}

void CGameActor::QueueWillBeAttacked(const WBA& hitInfo)
{
    m_willBeAttackedList.push_back(hitInfo);
}

u32 CGameActor::QueueDamageNumber(CGameActor* sourceActor, int value, u32 colorArgb, int kind, u32 delayMs)
{
    // Totals can exceed six digits even when each individual hit does not.
    const int clampedValue = kind == kTotalDamageNumberKind ? (std::max)(0, value) : ClampDamageNumber(value);
    if (kind == kTotalDamageNumberKind && delayMs == 0) {
        DeleteTotalNumber(kind);
    }
    if (CMsgEffect* effect = SpawnDamageNumberEffect(*this, sourceActor, clampedValue, colorArgb, kind)) {
        if (IsWhiteDamageNumber(kind, colorArgb)) {
            const u32 intervalMs = GetWhiteDamageNumberSettings().delayBetweenNumbersMs;
            if (m_hasNextWhiteDamageNumberTick && intervalMs > 0) {
                // Signed tick differences also handle the timer wrapping around.
                const s32 remainingMs = static_cast<s32>(m_nextWhiteDamageNumberTick - effect->m_stateStartTick);
                if (remainingMs > 0) {
                    delayMs = (std::max)(delayMs, static_cast<u32>(remainingMs));
                }
            }
            m_nextWhiteDamageNumberTick = effect->m_stateStartTick + delayMs + intervalMs;
            m_hasNextWhiteDamageNumberTick = intervalMs > 0;
        }
        effect->SetStartDelayMs(delayMs);
    }
    return delayMs;
}

void CGameActor::SetModifyFactorOfmotionSpeed(int attackMT)
{
    if (attackMT <= 0) {
        attackMT = 1440;
    }

    const float factor = static_cast<float>(attackMT) * kAttackMotionFactor;
    m_modifyFactorOfmotionSpeed = factor;
    m_modifyFactorOfmotionSpeed2 = factor;
}

int CGameActor::GetAttackMotion()
{
    CActRes* actRes = ResolveRuntimeActorActRes(this);
    if (m_isPc) {
        if (CActRes* transientActRes = ResolvePcTransientActRes(*this, m_curAction)) {
            actRes = transientActRes;
        }
    }
    if (!actRes) {
        return 0;
    }

    const int action = ResolveAvailableActionIndex(actRes, m_curAction);
    const int motionCount = actRes->GetMotionCount(action);
    for (int motionIndex = 0; motionIndex < motionCount; ++motionIndex) {
        const CMotion* motion = actRes->GetMotion(action, motionIndex);
        if (!motion || motion->eventId < 0) {
            continue;
        }

        const char* eventName = actRes->GetEventName(motion->eventId);
        if (eventName && std::strcmp(eventName, "atk") == 0) {
            return motionIndex;
        }
    }

    return motionCount > 1 ? motionCount - 2 : 0;
}

void CGameActor::RegisterPos() {
    g_world.RegisterActor(this);
}

void CGameActor::UnRegisterPos() {
    g_world.UnregisterActor(this);
}

void CGameActor::DeleteMatchingEffect(CMsgEffect* effect)
{
    if (!effect) {
        return;
    }

    if (m_birdEffect == effect) {
        m_birdEffect = nullptr;
    }
    if (m_cartEffect == effect) {
        DbgLog("[CartEffect] detach gid=%u effect=%p removedFromOwner=%d disappear=%d\n",
            static_cast<unsigned int>(m_gid),
            static_cast<void*>(effect),
            effect->m_removedFromOwner,
            effect->m_isDisappear);
        m_cartEffect = nullptr;
    }
    if (m_lastDamageNumberEffect == effect) {
        m_lastDamageNumberEffect = nullptr;
    }

    const auto it = std::find(m_msgEffectList.begin(), m_msgEffectList.end(), effect);
    if (it != m_msgEffectList.end()) {
        m_msgEffectList.erase(it);
    }
}

void CGameActor::DeleteTotalNumber(int kind, const CMsgEffect* keepEffect)
{
    std::vector<CMsgEffect*> matchingEffects;
    for (CMsgEffect* effect : m_msgEffectList) {
        if (!effect || effect->m_msgEffectType != kind || effect == keepEffect
            || (keepEffect && effect->m_startDelayMs > 0)) {
            continue;
        }
        matchingEffects.push_back(effect);
    }

    for (CMsgEffect* effect : matchingEffects) {
        DetachOwnedMsgEffect(*this, effect);
    }
}

namespace {

void ReleaseItemBillboardTexture(CItem& item)
{
    if (item.m_billboardTextureOwned && item.m_billboardTexture) {
        delete item.m_billboardTexture;
    }
    item.m_billboardTexture = nullptr;
    item.m_billboardTextureOwned = 0;
}

float DotVec3Item(const vector3d& a, const vector3d& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

vector3d NormalizeVec3Item(const vector3d& value)
{
    const float lengthSq = DotVec3Item(value, value);
    if (lengthSq <= 1.0e-12f) {
        return vector3d{ 0.0f, 1.0f, 0.0f };
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    return vector3d{ value.x * invLength, value.y * invLength, value.z * invLength };
}

vector3d ScaleVec3Item(const vector3d& value, float scale)
{
    return vector3d{ value.x * scale, value.y * scale, value.z * scale };
}

vector3d AddVec3Item(const vector3d& a, const vector3d& b)
{
    return vector3d{ a.x + b.x, a.y + b.y, a.z + b.z };
}

bool ProjectItemPoint(const matrix& viewMatrix, const vector3d& point, tlvertex3d* outVertex)
{
    if (!outVertex) {
        return false;
    }

    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        return false;
    }

    const float clipZ = point.x * viewMatrix.m[0][2]
        + point.y * viewMatrix.m[1][2]
        + point.z * viewMatrix.m[2][2]
        + viewMatrix.m[3][2];
    if (!std::isfinite(clipZ) || clipZ <= kItemProjectSubmitNearPlane) {
        return false;
    }

    const float oow = 1.0f / clipZ;
    const float projectedX = point.x * viewMatrix.m[0][0]
        + point.y * viewMatrix.m[1][0]
        + point.z * viewMatrix.m[2][0]
        + viewMatrix.m[3][0];
    const float projectedY = point.x * viewMatrix.m[0][1]
        + point.y * viewMatrix.m[1][1]
        + point.z * viewMatrix.m[2][1]
        + viewMatrix.m[3][1];
    if (!std::isfinite(oow) || !std::isfinite(projectedX) || !std::isfinite(projectedY)) {
        return false;
    }

    outVertex->x = g_renderer.m_xoffset + projectedX * g_renderer.m_hpc * oow;
    outVertex->y = g_renderer.m_yoffset + projectedY * g_renderer.m_vpc * oow;
    const float depth = (1500.0f / (1500.0f - kItemProjectNearPlane)) * ((1.0f / oow) - kItemProjectNearPlane) * oow;
    if (!std::isfinite(outVertex->x) || !std::isfinite(outVertex->y) || !std::isfinite(depth)) {
        return false;
    }

    outVertex->z = (std::min)(1.0f, (std::max)(0.0f, depth));
    outVertex->oow = oow;
    outVertex->specular = 0xFF000000u;
    return true;
}

bool ResolveItemSpritePaths(const std::string& resourceName, std::string* outActName, std::string* outSprName)
{
    if (outActName) {
        outActName->clear();
    }
    if (outSprName) {
        outSprName->clear();
    }
    if (resourceName.empty()) {
        return false;
    }

    const char* const roots[] = {
        kLegacyItemSpriteRoot,
        "sprite\\\xBE\xC6\xC0\xCC\xC5\xDB\\",
        "data\\sprite\\item\\",
        "sprite\\item\\",
        "data\\sprite\\items\\",
        "sprite\\items\\",
        nullptr
    };

    for (int index = 0; roots[index]; ++index) {
        const std::string actPath = std::string(roots[index]) + resourceName + ".act";
        const std::string sprPath = std::string(roots[index]) + resourceName + ".spr";
        if (g_resMgr.IsExist(actPath.c_str()) && g_resMgr.IsExist(sprPath.c_str())) {
            if (outActName) {
                *outActName = actPath;
            }
            if (outSprName) {
                *outSprName = sprPath;
            }
            return true;
        }
    }

    return false;
}

bool ResolveItemResources(CItem& item)
{
    if (!item.m_resourceName.empty()) {
        std::string actPath;
        std::string sprPath;
        if (ResolveItemSpritePaths(item.m_resourceName, &actPath, &sprPath)) {
            item.m_actRes = g_resMgr.GetAs<CActRes>(actPath.c_str());
            item.m_sprRes = g_resMgr.GetAs<CSprRes>(sprPath.c_str());
        } else {
            item.m_actRes = nullptr;
            item.m_sprRes = nullptr;
        }
    } else {
        item.m_actRes = nullptr;
        item.m_sprRes = nullptr;
    }

    if (item.m_actRes && item.m_sprRes) {
        return true;
    }

    static std::map<std::string, bool> loggedMissingItems;
    const std::string key = item.m_resourceName.empty()
        ? std::string("item:") + std::to_string(item.m_itemId)
        : item.m_resourceName;
    if (loggedMissingItems.insert(std::make_pair(key, true)).second) {
        DbgLog("[Item] missing floor item resources itemId=%u resource='%s'\n",
            static_cast<unsigned int>(item.m_itemId),
            item.m_resourceName.c_str());
    }
    return false;
}

bool EnsureItemBillboardTexture(CItem& item)
{
    if (!ResolveItemResources(item)) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    const bool isVulkanBackend = GetRenderDevice().GetBackendType() == RenderBackendType::Vulkan;

    const int motionCount = item.m_actRes->GetMotionCount(0);
    if (motionCount <= 0) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    int motion = item.m_curMotion;
    if (motion < 0 || motion >= motionCount) {
        motion = 0;
    }

    if (item.m_billboardTexture && item.m_cachedBillboardMotion == motion) {
        return true;
    }

    if (isVulkanBackend) {
        const SharedItemBillboardKey sharedKey{ item.m_itemId, item.m_identified, motion };
        const auto& sharedCache = GetSharedItemBillboardCache();
        const auto sharedIt = sharedCache.find(sharedKey);
        if (sharedIt != sharedCache.end() && sharedIt->second.texture) {
            if (item.m_billboardTexture != sharedIt->second.texture) {
                ReleaseItemBillboardTexture(item);
            }
            item.m_billboardTexture = sharedIt->second.texture;
            item.m_billboardTextureOwned = 0;
            item.m_billboardTextureWidth = sharedIt->second.width;
            item.m_billboardTextureHeight = sharedIt->second.height;
            item.m_billboardAnchorX = sharedIt->second.anchorX;
            item.m_billboardAnchorY = sharedIt->second.anchorY;
            item.m_billboardOpaqueBounds = sharedIt->second.opaqueBounds;
            item.m_cachedBillboardMotion = motion;
            return true;
        }
    }

    const CMotion* spriteMotion = item.m_actRes->GetMotion(0, motion);
    if (!spriteMotion) {
        spriteMotion = item.m_actRes->GetMotion(0, 0);
        motion = 0;
    }
    if (!spriteMotion) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    BillboardComposeSurface composeSurface(kItemBillboardComposeWidth, kItemBillboardComposeHeight);
    composeSurface.Clear(0x00000000u);
    composeSurface.BltSprite(kItemBillboardAnchorX,
        kItemBillboardAnchorY,
        item.m_sprRes,
        const_cast<CMotion*>(spriteMotion),
        item.m_sprRes->m_pal);

    tagRECT opaqueBounds{};
    if (!FindOpaqueBounds(composeSurface.GetImageData(),
            static_cast<int>(composeSurface.GetWidth()),
            static_cast<int>(composeSurface.GetHeight()),
            &opaqueBounds)) {
        ReleaseItemBillboardTexture(item);
        item.m_cachedBillboardMotion = -1;
        return false;
    }

    std::vector<unsigned int> pixels(composeSurface.GetImageData(),
        composeSurface.GetImageData() + static_cast<size_t>(composeSurface.GetWidth()) * static_cast<size_t>(composeSurface.GetHeight()));
    UnpremultiplyPixels(pixels);

    if (!item.m_billboardTexture
        || !item.m_billboardTextureOwned
        || item.m_billboardTextureWidth != kItemBillboardComposeWidth
        || item.m_billboardTextureHeight != kItemBillboardComposeHeight) {
        ReleaseItemBillboardTexture(item);
        item.m_billboardTexture = new CTexture();
        if (!item.m_billboardTexture) {
            return false;
        }
        if (!item.m_billboardTexture->Create(kItemBillboardComposeWidth, kItemBillboardComposeHeight, PF_A8R8G8B8, false)) {
            ReleaseItemBillboardTexture(item);
            return false;
        }
        SetTextureDebugName(item.m_billboardTexture, "__item_billboard__");
        item.m_billboardTextureOwned = 1;
        item.m_billboardTextureWidth = kItemBillboardComposeWidth;
        item.m_billboardTextureHeight = kItemBillboardComposeHeight;
    }

    item.m_billboardTexture->Update(0,
        0,
        kItemBillboardComposeWidth,
        kItemBillboardComposeHeight,
        pixels.data(),
        true,
        kItemBillboardComposeWidth * static_cast<int>(sizeof(unsigned int)));
    item.m_billboardAnchorX = kItemBillboardAnchorX;
    item.m_billboardAnchorY = kItemBillboardAnchorY;
    item.m_billboardOpaqueBounds = opaqueBounds;
    item.m_cachedBillboardMotion = motion;

    if (isVulkanBackend && item.m_billboardTexture) {
        SharedItemBillboardValue value{};
        value.texture = item.m_billboardTexture;
        value.opaqueBounds = opaqueBounds;
        value.width = item.m_billboardTextureWidth;
        value.height = item.m_billboardTextureHeight;
        value.anchorX = item.m_billboardAnchorX;
        value.anchorY = item.m_billboardAnchorY;
        GetSharedItemBillboardCache()[SharedItemBillboardKey{ item.m_itemId, item.m_identified, motion }] = value;
        item.m_billboardTextureOwned = 0;
    }

    return true;
}

} // namespace

bool CItem::EnsureBillboardTexture()
{
    return EnsureItemBillboardTexture(*this);
}

CItem::CItem()
    : m_aid(0)
    , m_itemId(0)
    , m_amount(0)
    , m_tileX(0)
    , m_tileY(0)
    , m_identified(0)
    , m_subX(0)
    , m_subY(0)
    , m_isJumping(0)
    , m_sfallingSpeed(0.0f)
    , m_sPosY(0.0f)
    , m_billboardTexture(nullptr)
    , m_billboardTextureOwned(0)
    , m_billboardTextureWidth(0)
    , m_billboardTextureHeight(0)
    , m_billboardAnchorX(kItemBillboardAnchorX)
    , m_billboardAnchorY(kItemBillboardAnchorY)
    , m_cachedBillboardMotion(-1)
{
    m_billboardOpaqueBounds.left = 0;
    m_billboardOpaqueBounds.top = 0;
    m_billboardOpaqueBounds.right = 0;
    m_billboardOpaqueBounds.bottom = 0;
    m_isVisible = 1;
    m_isVisibleBody = 1;
    m_shouldAddPickInfo = 1;
    m_motionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed2 = 1.0f;
    m_baseAction = 0;
    m_curAction = 0;
    m_curMotion = 0;
    m_oldBaseAction = 0;
    m_oldMotion = 0;
    m_stateStartTick = timeGetTime();
    m_pos = vector3d{ 0.0f, 0.0f, 0.0f };
}

CItem::~CItem()
{
    ReleaseItemBillboardTexture(*this);
}

u8 CItem::OnProcess()
{
    if (m_isJumping) {
        m_sfallingSpeed += 0.18f;
        m_sPosY -= m_sfallingSpeed;
        if (m_sPosY <= 0.0f) {
            if (m_sfallingSpeed > 0.45f) {
                m_sPosY = -m_sPosY * 0.35f;
                m_sfallingSpeed = -m_sfallingSpeed * 0.45f;
            } else {
                m_sPosY = 0.0f;
                m_sfallingSpeed = 0.0f;
                m_isJumping = 0;
            }
        }
    }

    if (ResolveItemResources(*this)) {
        ProcessMotion();
    } else {
        m_curMotion = 0;
    }

    return 1;
}

void CItem::Render(matrix* viewMatrix)
{
    if (!viewMatrix || !m_isVisible || !EnsureBillboardTexture()) {
        return;
    }

    vector3d base = m_pos;
    if (g_world.m_attr) {
        base.y = g_world.m_attr->GetHeight(base.x, base.z);
    }
    base.y += m_sPosY;

    tlvertex3d projectedBase{};
    if (!ProjectItemPoint(*viewMatrix, base, &projectedBase)) {
        return;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return;
    }

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = m_billboardTexture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = projectedBase.oow;

    const float scaledAnchorX = static_cast<float>(m_billboardAnchorX) * kGroundItemScreenScale;
    const float scaledAnchorY = static_cast<float>(m_billboardAnchorY) * kGroundItemScreenScale;
    const float scaledWidth = static_cast<float>(m_billboardTextureWidth) * kGroundItemScreenScale;
    const float scaledHeight = static_cast<float>(m_billboardTextureHeight) * kGroundItemScreenScale;
    const float left = projectedBase.x - scaledAnchorX;
    const float top = projectedBase.y - scaledAnchorY;
    const float right = left + scaledWidth;
    const float bottom = top + scaledHeight;

    face->m_verts[0].x = left;
    face->m_verts[0].y = top;
    face->m_verts[1].x = right;
    face->m_verts[1].y = top;
    face->m_verts[2].x = left;
    face->m_verts[2].y = bottom;
    face->m_verts[3].x = right;
    face->m_verts[3].y = bottom;

    for (int index = 0; index < 4; ++index) {
        face->m_verts[index].z = (std::max)(0.0f, projectedBase.z - kItemBillboardDepthBias);
        face->m_verts[index].oow = projectedBase.oow;
        face->m_verts[index].color = 0xFFFFFFFFu;
        face->m_verts[index].specular = 0xFF000000u;
    }

    face->m_verts[0].tu = 0.0f;
    face->m_verts[0].tv = 0.0f;
    face->m_verts[1].tu = 1.0f;
    face->m_verts[1].tv = 0.0f;
    face->m_verts[2].tu = 0.0f;
    face->m_verts[2].tv = 1.0f;
    face->m_verts[3].tu = 1.0f;
    face->m_verts[3].tv = 1.0f;
    g_renderer.AddRP(face, 1);
}

int CItem::Get8Dir(float rot)
{
    (void)rot;
    return 0;
}

void CItem::InvalidateBillboard()
{
    m_cachedBillboardMotion = -1;
}

void CItem::TriggerDropAnimation()
{
    m_isJumping = 1;
    m_sPosY = 2.2f;
    m_sfallingSpeed = -0.28f;
}

CPc::CPc()
    : m_honor(0)
    , m_virtue(0)
    , m_headDir(0)
    , m_head(0)
    , m_headPalette(0)
    , m_weapon(0)
    , m_accessory(0)
    , m_accessory2(0)
    , m_accessory3(0)
    , m_shield(0)
    , m_shoe(0)
    , m_shoe_count(0)
    , m_renderWithoutLayer(0)
    , m_gage(nullptr)
    , m_pk_rank(0)
    , m_pk_total(0)
    , m_billboardTexture(nullptr)
    , m_billboardTextureOwned(0)
    , m_billboardTextureWidth(0)
    , m_billboardTextureHeight(0)
    , m_billboardAnchorX(kPlayerBillboardAnchorX)
    , m_billboardAnchorY(kPlayerBillboardAnchorY)
    , m_cachedBillboardBodyAction(-1)
    , m_cachedBillboardHeadMotion(-1)
    , m_cachedBillboardJob(-1)
    , m_cachedBillboardHead(-1)
    , m_cachedBillboardSex(-1)
    , m_cachedBillboardBodyPalette(-1)
    , m_cachedBillboardBodyStyle(-1)
    , m_cachedBillboardHeadPalette(-1)
    , m_cachedBillboardWeapon(-1)
    , m_cachedBillboardShield(-1)
    , m_cachedBillboardAccessory(-1)
    , m_cachedBillboardAccessory2(-1)
    , m_cachedBillboardAccessory3(-1)
    , m_cachedBillboardAccessoryMotions{ -1, -1, -1 }
    , m_accessoryAnimationStartTick(timeGetTime())
    , m_cachedNonPcResourceJob(-1)
    , m_cachedNonPcActRes(nullptr)
    , m_cachedNonPcSprRes(nullptr)
{
    m_billboardOpaqueBounds.left = 0;
    m_billboardOpaqueBounds.top = 0;
    m_billboardOpaqueBounds.right = 0;
    m_billboardOpaqueBounds.bottom = 0;
    m_sprRes = nullptr;
    m_actRes = nullptr;
    m_motionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed = 1.0f;
    m_modifyFactorOfmotionSpeed2 = 1.0f;
    m_attackMotion = -1.0f;
    m_isSitting = 0;
    m_curAction = 0;
    m_baseAction = 0;
    m_curMotion = 0;
    m_oldBaseAction = 0;
    m_oldMotion = 0;
    m_isPlayHitWave = 0;
    m_hitWaveName[0] = '\0';
}

CPc::~CPc()
{
    if (m_gage) {
        g_windowMgr.DeleteWindow(m_gage);
        m_gage = nullptr;
    }

    ReleaseActorBillboardTexture(*this);
}

void CPc::SetState(int state)
{
    if (m_isPc == 0) {
        CGameActor::SetState(state);
        InvalidateBillboard();
        return;
    }

    const int previousState = m_stateId;
    if (previousState == kDeathStateId || m_isTrickDead || state == -1) {
        return;
    }

    if (previousState == kHitReactionStateId) {
        m_pos.x = m_damageDestX;
        m_pos.z = m_damageDestZ;
    }

    switch (state) {
    case 0:
        CGameActor::SetState(0);
        m_curMotion = m_headDir;
        if (previousState == kHitReactionStateId
            || previousState == kAttackStateId
            || previousState == kSecondAttackStateId
            || m_pkState
            || previousState == kGameActorCastingStateId) {
            SetAction(32, 4, 0);
            m_curAction = m_baseAction + Get8Dir(m_roty);
        }
        break;
    case kMoveStateId:
        m_isCounter = 0;
        if (m_bodyState != 1 && m_bodyState != 2) {
            m_isMotionFreezed = 0;
        }
        m_headDir = 0;
        CGameActor::SetState(kMoveStateId);
        break;
    case kAttackStateId:
    case kSecondAttackStateId: {
        m_isCounter = 0;
        if (m_bodyState != 1 && m_bodyState != 2) {
            m_isMotionFreezed = 0;
        }
        SendMsg(this, 83, 0, 0, 0);

        const bool secondAttack = state == kSecondAttackStateId || ResolvePcAttackAction(*this) == 88;
        CGameActor::SetState(secondAttack ? kSecondAttackStateId : kAttackStateId);
        break;
    }
    default:
        CGameActor::SetState(state);
        break;
    }

    InvalidateBillboard();
}

void CPc::SetModifyFactorOfmotionSpeed(int attackMT)
{
    if (m_isPc == 0) {
        CGameActor::SetModifyFactorOfmotionSpeed(attackMT);
        return;
    }

    if (attackMT <= 0) {
        attackMT = 1440;
    }

    m_modifyFactorOfmotionSpeed = static_cast<float>(attackMT) * kAttackMotionFactor;
    m_modifyFactorOfmotionSpeed2 = m_modifyFactorOfmotionSpeed;
}

void CPc::InvalidateBillboard()
{
    m_cachedBillboardBodyAction = -1;
    m_cachedBillboardHeadMotion = -1;
    m_cachedBillboardJob = -1;
    m_cachedBillboardHead = -1;
    m_cachedBillboardSex = -1;
    m_cachedBillboardBodyPalette = -1;
    m_cachedBillboardBodyStyle = -1;
    m_cachedBillboardHeadPalette = -1;
    m_cachedBillboardWeapon = -1;
    m_cachedBillboardShield = -1;
    m_cachedBillboardAccessory = -1;
    m_cachedBillboardAccessory2 = -1;
    m_cachedBillboardAccessory3 = -1;
    m_cachedBillboardAccessoryMotions.fill(-1);
    m_cachedBillboardAnimation = {};
    const int displayJob = ResolveDisplayJob(*this);
    if ((!m_isPc || displayJob != m_job) && m_cachedNonPcResourceJob != displayJob) {
        m_cachedNonPcResourceJob = -1;
        m_cachedNonPcActRes = nullptr;
        m_cachedNonPcSprRes = nullptr;
    }
}

namespace {

bool ResolveCachedNonPcResourcesForActor(CPc& actor, CActRes** outActRes, CSprRes** outSprRes)
{
    if (!outActRes || !outSprRes) {
        return false;
    }

    const int displayJob = ResolveDisplayJob(actor);

    if (actor.m_cachedNonPcResourceJob != displayJob
        || !actor.m_cachedNonPcActRes
        || !actor.m_cachedNonPcSprRes) {
        char actPath[260] = {};
        char sprPath[260] = {};
        const char* const jobName = g_session.GetJobName(displayJob);
        if (!ResolveNonPcSpritePaths(displayJob, actPath, sprPath)) {
            LogNonPcResourceMissingOnce(displayJob, jobName, actPath, sprPath, nullptr, nullptr);
            actor.m_cachedNonPcResourceJob = -1;
            actor.m_cachedNonPcActRes = nullptr;
            actor.m_cachedNonPcSprRes = nullptr;
            return false;
        }

        actor.m_cachedNonPcActRes = g_resMgr.GetAs<CActRes>(actPath);
        actor.m_cachedNonPcSprRes = g_resMgr.GetAs<CSprRes>(sprPath);
        if (!actor.m_cachedNonPcActRes || !actor.m_cachedNonPcSprRes) {
            LogNonPcResourceMissingOnce(displayJob,
                jobName,
                actPath,
                sprPath,
                actor.m_cachedNonPcActRes,
                actor.m_cachedNonPcSprRes);
        } else {
            LogNonPcResourceResolvedOnce(displayJob, jobName, actPath, sprPath);
        }
        actor.m_cachedNonPcResourceJob = (actor.m_cachedNonPcActRes && actor.m_cachedNonPcSprRes)
            ? displayJob
            : -1;
    }

    *outActRes = actor.m_cachedNonPcActRes;
    *outSprRes = actor.m_cachedNonPcSprRes;
    return *outActRes != nullptr && *outSprRes != nullptr;
}

CActRes* ResolveRuntimeActorActRes(CRenderObject* object)
{
    if (!object) {
        return nullptr;
    }

    CGameActor* actor = dynamic_cast<CGameActor*>(object);
    if (!actor) {
        return object->m_actRes;
    }

    if (actor->m_isPc) {
        const int displayJob = ResolveDisplayJob(*actor);
        const int sex = actor->m_sex != 0 ? 1 : 0;
        char bodyActPath[260] = {};
        const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyActPath, actor->m_bodyStyle);
        actor->m_actRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
        return actor->m_actRes;
    }

    if (CPc* pcActor = dynamic_cast<CPc*>(actor)) {
        CActRes* actRes = nullptr;
        CSprRes* sprRes = nullptr;
        if (ResolveCachedNonPcResourcesForActor(*pcActor, &actRes, &sprRes)) {
            actor->m_actRes = actRes;
            actor->m_sprRes = sprRes;
            return actRes;
        }
    }

    return actor->m_actRes;
}

} // namespace

bool CPc::EnsureBillboardTexture(float cameraLongitude)
{
    if (IsStatusAnimationFrozen() && m_billboardTexture) {
        // Hold the complete displayed pose, including interpolated layers and
        // independently animated accessories, until the server clears opt1.
        return true;
    }
    const bool isVulkanBackend = GetRenderDevice().GetBackendType() == RenderBackendType::Vulkan;
    const bool useSharedBillboardCache = GetRenderDevice().GetBackendType() != RenderBackendType::LegacyDirect3D7;

    const float actorRotationDegrees = std::isfinite(m_roty)
        ? NormalizeAngle360(m_roty)
        : ActorRotationDegreesFromDir(ResolvePcFacingDir(this));
    const bool isPlayerStyleActor = m_isPc != 0;
    const bool usePlayerStyleBillboard = isPlayerStyleActor;
    const int displayJob = ResolveDisplayJob(*this);
    const int sex = m_sex != 0 ? 1 : 0;
    SharedPlayerBillboardKey sharedPlayerKey = BuildSharedPlayerBillboardKey(*this, displayJob, sex);
    const int baseBodyAction = m_isSitting ? 16 : (m_isMoving ? 8 : 0);
    int previousDir = -1;
    if (m_cachedBillboardBodyAction >= baseBodyAction && m_cachedBillboardBodyAction < baseBodyAction + 8) {
        previousDir = m_cachedBillboardBodyAction - baseBodyAction;
    }
    int bodyAction = baseBodyAction + ResolvePcBodyActionFromViewStable(cameraLongitude, actorRotationDegrees, previousDir);
    int headMotion = 0;
    CActRes* samplingActRes = nullptr;
    const bool isLocalPlayer = m_gid != 0 && (m_gid == g_session.m_gid || m_gid == g_session.m_aid);
    if (usePlayerStyleBillboard) {
        char bodyAct[260] = {};
        const std::string bodyActName = g_session.GetJobActName(displayJob, sex, bodyAct, m_bodyStyle);
        CActRes* bodyActRes = ResolvePcTransientActRes(*this, m_curAction);
        if (!bodyActRes) {
            bodyActRes = g_resMgr.GetAs<CActRes>(bodyActName.c_str());
        }
        samplingActRes = bodyActRes;

        auto tryUseRuntimeDirectionalAction = [&](int runtimeBaseAction, int outputBaseAction) {
            if (m_curAction < runtimeBaseAction || m_curAction >= runtimeBaseAction + 8) {
                return false;
            }
            bodyAction = outputBaseAction + (m_curAction - runtimeBaseAction);
            return true;
        };

        if (IsTransientActionActive(*this, bodyActRes, m_curAction)) {
            bodyAction = m_curAction;
            headMotion = m_curMotion;
        } else {
            bool usedRuntimeDirectionalAction = false;
            if (m_isMoving) {
                usedRuntimeDirectionalAction = tryUseRuntimeDirectionalAction(8, 8);
            } else if (m_stateId == kMoveStateId) {
                // Preserve the last walk-facing direction for the first idle frame
                // after movement stops so the billboard does not flip for one tick.
                usedRuntimeDirectionalAction = tryUseRuntimeDirectionalAction(8, 0);
            }

            if (usedRuntimeDirectionalAction && m_isMoving) {
                headMotion = ResolvePcMotionIndex(this, bodyAction, bodyActName);
            } else {
                headMotion = ResolveHeadMotionFromBodyAction(bodyAction, m_headDir);
                if (headMotion < 0) {
                    headMotion = ResolvePcMotionIndex(this, bodyAction, bodyActName);
                }
            }
        }
    } else {
        CActRes* actRes = nullptr;
        CSprRes* sprRes = nullptr;
        if (ResolveCachedNonPcResourcesForActor(*this, &actRes, &sprRes) && actRes) {
            samplingActRes = actRes;
            const bool transientActionActive = IsTransientActionActive(*this, actRes, m_curAction);
            if (transientActionActive) {
                bodyAction = m_curAction;
                headMotion = m_curMotion;
                LogDeathBillboardSelectionOnce(*this, actRes, bodyAction, headMotion);
            } else {
                const int directionalAction = m_baseAction + Get8Dir(m_roty);
                const int resolvedDirectionalAction = ResolveAvailableActionIndex(actRes, directionalAction);
                if (resolvedDirectionalAction >= 0) {
                    bodyAction = resolvedDirectionalAction;
                } else {
                    bodyAction = directionalAction;
                }
                headMotion = ResolveSpriteMotionIndex(*this, actRes, bodyAction);
                ProcessRenderMotionWaveEvents(this, actRes, bodyAction, headMotion);
            }
        }
    }

    ActorBillboardAnimation animation;
    animation.enabled = GetCachedGraphicsSettings().spriteInterpolation;
    animation.sharpPixels = GetCachedGraphicsSettings().sharpSprites;
    animation.sharpness = animation.sharpPixels ? GetCachedGraphicsSettings().spriteSharpness : 4;
    animation.body = ResolveActorRenderSample(*this, samplingActRes, bodyAction, headMotion);
    if (usePlayerStyleBillboard) {
        animation.accessories = ResolvePcAccessoryMotionSamples(*this, bodyAction,
            animation.body, timeGetTime() - m_accessoryAnimationStartTick);
        for (size_t slot = 0; slot < animation.accessories.size(); ++slot) {
            sharedPlayerKey.accessoryMotions[slot] = animation.accessories[slot].current;
        }
    }
    sharedPlayerKey.animation = animation;

    if (m_billboardTexture
        && m_cachedBillboardBodyAction == bodyAction
        && m_cachedBillboardHeadMotion == headMotion
        && m_cachedBillboardAccessoryMotions == sharedPlayerKey.accessoryMotions
        && m_cachedBillboardAnimation == animation
        && m_cachedBillboardJob == displayJob
        && m_cachedBillboardHead == (usePlayerStyleBillboard ? m_head : 0)
        && m_cachedBillboardSex == sex
        && m_cachedBillboardBodyPalette == (usePlayerStyleBillboard ? m_bodyPalette : 0)
        && m_cachedBillboardBodyStyle == (usePlayerStyleBillboard ? m_bodyStyle : 0)
        && m_cachedBillboardHeadPalette == (usePlayerStyleBillboard ? m_headPalette : 0)
        && m_cachedBillboardWeapon == (usePlayerStyleBillboard ? m_weapon : 0)
        && m_cachedBillboardShield == (usePlayerStyleBillboard ? m_shield : 0)
        && m_cachedBillboardAccessory == (usePlayerStyleBillboard ? m_accessory : 0)
        && m_cachedBillboardAccessory2 == (usePlayerStyleBillboard ? m_accessory2 : 0)
        && m_cachedBillboardAccessory3 == (usePlayerStyleBillboard ? m_accessory3 : 0)) {
        if (kLogBillboardDiagnostics && isLocalPlayer) {
            static std::map<u32, u32> loggedBillboardCacheHits;
            const u32 cacheKey = (static_cast<u32>(displayJob & 0xFFFF) << 16)
                ^ (static_cast<u32>(bodyAction & 0xFF) << 8)
                ^ static_cast<u32>(headMotion & 0xFF)
                ^ (usePlayerStyleBillboard ? 0x80000000u : 0u);
            if (loggedBillboardCacheHits[m_gid] != cacheKey) {
                loggedBillboardCacheHits[m_gid] = cacheKey;
                DbgLog("[ActorTrace] billboard cache reuse gid=%u playerStyle=%d displayJob=%d state=%d bodyAction=%d headMotion=%d bodyPal=%d headPal=%d weapon=%d shield=%d\n",
                    m_gid,
                    usePlayerStyleBillboard ? 1 : 0,
                    displayJob,
                    m_stateId,
                    bodyAction,
                    headMotion,
                    m_bodyPalette,
                    m_headPalette,
                    m_weapon,
                    m_shield);
            }
        }
        return true;
    }

    const bool isTransientBillboard = bodyAction < baseBodyAction || bodyAction >= baseBodyAction + 8;

    if (useSharedBillboardCache && !usePlayerStyleBillboard) {
        PopulateSharedNonPcBillboardFrame(*this, displayJob, bodyAction, headMotion, animation);

        const SharedNonPcBillboardKey sharedKey{ displayJob, bodyAction, headMotion, animation };
        const auto& sharedCache = GetSharedNonPcBillboardCache();
        const auto sharedIt = sharedCache.find(sharedKey);
        if (sharedIt != sharedCache.end() && sharedIt->second.texture) {
            if (m_billboardTexture != sharedIt->second.texture.get()) {
                ReleaseActorBillboardTexture(*this);
            }
            m_sharedBillboardTexture = sharedIt->second.texture;
            m_billboardTexture = m_sharedBillboardTexture.get();
            m_billboardTextureOwned = 0;
            m_billboardTextureWidth = sharedIt->second.width;
            m_billboardTextureHeight = sharedIt->second.height;
            m_billboardAnchorX = sharedIt->second.anchorX;
            m_billboardAnchorY = sharedIt->second.anchorY;
            m_billboardOpaqueBounds = sharedIt->second.opaqueBounds;
            m_cachedBillboardBodyAction = bodyAction;
            m_cachedBillboardHeadMotion = headMotion;
            m_cachedBillboardJob = displayJob;
            m_cachedBillboardHead = 0;
            m_cachedBillboardSex = sex;
            m_cachedBillboardBodyPalette = 0;
            m_cachedBillboardBodyStyle = 0;
            m_cachedBillboardHeadPalette = 0;
            m_cachedBillboardWeapon = 0;
            m_cachedBillboardShield = 0;
            m_cachedBillboardAccessory = 0;
            m_cachedBillboardAccessory2 = 0;
            m_cachedBillboardAccessory3 = 0;
            m_cachedBillboardAccessoryMotions = sharedPlayerKey.accessoryMotions;
            m_cachedBillboardAnimation = animation;
            return true;
        }
    }

    if (useSharedBillboardCache && usePlayerStyleBillboard) {
        PopulateSharedPlayerBillboardFrame(*this, sharedPlayerKey, displayJob, sex, bodyAction, headMotion);

        SharedPlayerBillboardKey sharedKey = sharedPlayerKey;
        sharedKey.action = bodyAction;
        sharedKey.motion = headMotion;
        const auto& sharedCache = GetSharedPlayerBillboardCache();
        const auto sharedIt = sharedCache.find(sharedKey);
        if (sharedIt != sharedCache.end() && sharedIt->second.texture) {
            if (m_billboardTexture != sharedIt->second.texture.get()) {
                ReleaseActorBillboardTexture(*this);
            }
            m_sharedBillboardTexture = sharedIt->second.texture;
            m_billboardTexture = m_sharedBillboardTexture.get();
            m_billboardTextureOwned = 0;
            m_billboardTextureWidth = sharedIt->second.width;
            m_billboardTextureHeight = sharedIt->second.height;
            m_billboardAnchorX = sharedIt->second.anchorX;
            m_billboardAnchorY = sharedIt->second.anchorY;
            m_billboardOpaqueBounds = sharedIt->second.opaqueBounds;
            m_cachedBillboardBodyAction = bodyAction;
            m_cachedBillboardHeadMotion = headMotion;
            m_cachedBillboardJob = displayJob;
            m_cachedBillboardHead = m_head;
            m_cachedBillboardSex = sex;
            m_cachedBillboardBodyPalette = m_bodyPalette;
            m_cachedBillboardBodyStyle = m_bodyStyle;
            m_cachedBillboardHeadPalette = m_headPalette;
            m_cachedBillboardWeapon = m_weapon;
            m_cachedBillboardShield = m_shield;
            m_cachedBillboardAccessory = m_accessory;
            m_cachedBillboardAccessory2 = m_accessory2;
            m_cachedBillboardAccessory3 = m_accessory3;
            m_cachedBillboardAccessoryMotions = sharedPlayerKey.accessoryMotions;
            m_cachedBillboardAnimation = animation;
            return true;
        }
    }

    BillboardComposeConfig composeConfig{};
    if (usePlayerStyleBillboard) {
        if (!ResolvePlayerBillboardComposeConfig(*this, bodyAction, headMotion,
                sharedPlayerKey.accessoryMotions, animation, &composeConfig)) {
            return false;
        }
    } else {
        ResolveNonPcBillboardComposeConfig(*this, bodyAction, headMotion, &composeConfig, animation);
    }
    const int composeWidth = composeConfig.width;
    const int composeHeight = composeConfig.height;
    const int composeAnchorX = composeConfig.anchorX;
    const int composeAnchorY = composeConfig.anchorY;

    BillboardComposeSurface composeSurface(composeWidth, composeHeight, animation.sharpPixels);
    composeSurface.animation = animation;
    composeSurface.Clear(0x00000000u);

    int resolvedJob = -1;
    int resolvedHead = -1;
    int resolvedSex = -1;
    int resolvedBodyPalette = -1;
    int resolvedHeadPalette = -1;
    const bool drawOk = usePlayerStyleBillboard
        ? DrawPcBillboard(composeSurface,
            *this,
            composeAnchorX,
            composeAnchorY,
            bodyAction,
            headMotion,
            sharedPlayerKey.accessoryMotions,
            &resolvedJob,
            &resolvedHead,
            &resolvedSex,
            &resolvedBodyPalette,
            &resolvedHeadPalette)
        : DrawNonPcBillboard(composeSurface,
            *this,
            composeAnchorX,
            composeAnchorY,
            bodyAction,
            headMotion,
            &resolvedJob);
    if (!drawOk) {
        if (!usePlayerStyleBillboard) {
            LogNonPcBillboardFailureOnce(displayJob, g_session.GetJobName(displayJob), bodyAction, headMotion);
        }
        return false;
    }

    if (kLogBillboardDiagnostics && isLocalPlayer) {
        static std::map<u32, u32> loggedBillboardCompose;
        const u32 composeKey = (static_cast<u32>(displayJob & 0xFFFF) << 16)
            ^ (static_cast<u32>(bodyAction & 0xFF) << 8)
            ^ static_cast<u32>(headMotion & 0xFF)
            ^ (usePlayerStyleBillboard ? 0x40000000u : 0u);
        if (loggedBillboardCompose[m_gid] != composeKey) {
            loggedBillboardCompose[m_gid] = composeKey;
            DbgLog("[ActorTrace] billboard compose gid=%u playerStyle=%d displayJob=%d resolvedJob=%d state=%d bodyAction=%d headMotion=%d roty=%.1f dir=%d bodyPal=%d headPal=%d weapon=%d shield=%d\n",
                m_gid,
                usePlayerStyleBillboard ? 1 : 0,
                displayJob,
                resolvedJob,
                m_stateId,
                bodyAction,
                headMotion,
                m_roty,
                Get8Dir(m_roty),
                m_bodyPalette,
                m_headPalette,
                m_weapon,
                m_shield);
        }
    }

    if (!usePlayerStyleBillboard) {
        resolvedHead = 0;
        resolvedSex = sex;
        resolvedBodyPalette = 0;
        resolvedHeadPalette = 0;
    }

    tagRECT opaqueBounds{};
    if (!FindOpaqueBounds(composeSurface.GetImageData(),
            static_cast<int>(composeSurface.GetWidth()),
            static_cast<int>(composeSurface.GetHeight()),
            &opaqueBounds)) {
        return false;
    }
    composeSurface.ToLogicalBounds(&opaqueBounds);

    std::vector<unsigned int> pixels(composeSurface.GetImageData(),
        composeSurface.GetImageData() + static_cast<size_t>(composeSurface.GetWidth()) * static_cast<size_t>(composeSurface.GetHeight()));
    UnpremultiplyPixels(pixels);

    if (isVulkanBackend) {
        static DWORD s_vulkanBillboardUploadTick = 0;
        static unsigned int s_vulkanBillboardUploadsThisTick = 0;
        const DWORD tick = timeGetTime();
        if (tick != s_vulkanBillboardUploadTick) {
            s_vulkanBillboardUploadTick = tick;
            s_vulkanBillboardUploadsThisTick = 0;
        }
        if (s_vulkanBillboardUploadsThisTick >= 4u && !isTransientBillboard) {
            return m_billboardTexture != nullptr;
        }
        ++s_vulkanBillboardUploadsThisTick;
    }

    if (!m_billboardTexture
        || !m_billboardTextureOwned
        || m_billboardTextureWidth != composeWidth
        || m_billboardTextureHeight != composeHeight
        || m_billboardTexture->m_updateWidth != static_cast<unsigned int>(composeSurface.GetWidth())
        || m_billboardTexture->m_updateHeight != static_cast<unsigned int>(composeSurface.GetHeight())) {
        ReleaseActorBillboardTexture(*this);
        m_billboardTexture = new CTexture();
        if (!m_billboardTexture) {
            return false;
        }
        if (!m_billboardTexture->Create(composeSurface.GetWidth(), composeSurface.GetHeight(), PF_A8R8G8B8, false)) {
            ReleaseActorBillboardTexture(*this);
            return false;
        }
        m_billboardTextureOwned = 1;
        m_billboardTextureWidth = composeWidth;
        m_billboardTextureHeight = composeHeight;
    }

    SetActorBillboardDebugName(m_billboardTexture,
        *this,
        displayJob,
        bodyAction,
        headMotion,
        usePlayerStyleBillboard);

    m_billboardTexture->Update(0,
        0,
        composeSurface.GetWidth(),
        composeSurface.GetHeight(),
        pixels.data(),
        true,
        composeSurface.GetWidth() * static_cast<int>(sizeof(unsigned int)));

    m_billboardAnchorX = composeAnchorX;
    m_billboardAnchorY = composeAnchorY;
    m_billboardOpaqueBounds = opaqueBounds;
    m_cachedBillboardBodyAction = bodyAction;
    m_cachedBillboardHeadMotion = headMotion;
    m_cachedBillboardJob = resolvedJob;
    m_cachedBillboardHead = resolvedHead;
    m_cachedBillboardSex = resolvedSex;
    m_cachedBillboardBodyPalette = resolvedBodyPalette;
    m_cachedBillboardBodyStyle = usePlayerStyleBillboard ? m_bodyStyle : 0;
    m_cachedBillboardHeadPalette = resolvedHeadPalette;
    m_cachedBillboardWeapon = usePlayerStyleBillboard ? m_weapon : 0;
    m_cachedBillboardShield = usePlayerStyleBillboard ? m_shield : 0;
    m_cachedBillboardAccessory = usePlayerStyleBillboard ? m_accessory : 0;
    m_cachedBillboardAccessory2 = usePlayerStyleBillboard ? m_accessory2 : 0;
    m_cachedBillboardAccessory3 = usePlayerStyleBillboard ? m_accessory3 : 0;
    m_cachedBillboardAccessoryMotions = sharedPlayerKey.accessoryMotions;
    m_cachedBillboardAnimation = animation;

    if (useSharedBillboardCache && !usePlayerStyleBillboard && m_billboardTexture) {
        SharedNonPcBillboardValue value{};
        m_sharedBillboardTexture.reset(m_billboardTexture);
        value.texture = m_sharedBillboardTexture;
        value.lastUsed = NextBillboardCacheUse();
        value.opaqueBounds = opaqueBounds;
        value.width = m_billboardTextureWidth;
        value.height = m_billboardTextureHeight;
        value.anchorX = m_billboardAnchorX;
        value.anchorY = m_billboardAnchorY;

        GetSharedNonPcBillboardCache()[SharedNonPcBillboardKey{ displayJob, bodyAction, headMotion, animation }] = value;
        m_billboardTextureOwned = 0;
        TrimSharedBillboardCache(GetSharedNonPcBillboardCache());
    }

    if (useSharedBillboardCache && usePlayerStyleBillboard && m_billboardTexture) {
        SharedPlayerBillboardValue value{};
        m_sharedBillboardTexture.reset(m_billboardTexture);
        value.texture = m_sharedBillboardTexture;
        value.lastUsed = NextBillboardCacheUse();
        value.opaqueBounds = opaqueBounds;
        value.width = m_billboardTextureWidth;
        value.height = m_billboardTextureHeight;
        value.anchorX = m_billboardAnchorX;
        value.anchorY = m_billboardAnchorY;

        SharedPlayerBillboardKey sharedKey = sharedPlayerKey;
        sharedKey.action = bodyAction;
        sharedKey.motion = headMotion;
        GetSharedPlayerBillboardCache()[sharedKey] = value;
        m_billboardTextureOwned = 0;
        TrimSharedBillboardCache(GetSharedPlayerBillboardCache());
    }

    return true;
}

void CPc::WarmupCommonBillboardCache()
{
    if (m_isPc == 0 || GetRenderDevice().GetBackendType() != RenderBackendType::Vulkan) {
        return;
    }

    const int displayJob = ResolveDisplayJob(*this);
    const int sex = m_sex != 0 ? 1 : 0;
    const SharedPlayerBillboardKey sharedPlayerKey = BuildSharedPlayerBillboardKey(*this, displayJob, sex);
    const std::array<int, 3> baseActions{ 0, 8, 16 };

    for (const int baseAction : baseActions) {
        for (int dir = 0; dir < 8; ++dir) {
            PrimePlayerBillboardStrip(*this, sharedPlayerKey, displayJob, sex, baseAction + dir);
        }
    }
}

CPlayer::CPlayer()
    : m_destCellX(-1)
    , m_destCellZ(-1)
    , m_attackReqTime(0)
    , m_preMoveStartTick(0)
    , m_preMoveOn(0)
    , m_attackMode(0)
    , m_isAttackRequest(0)
    , m_isWaitingMoveAck(0)
    , m_isPreengageStateOfMove(0)
    , m_proceedTargetGid(0)
    , m_totalAttackReqCnt(0)
    , m_tickOfMoveForAttack(0)
    , m_moveReqTick(0)
    , m_standTick(0)
    , m_skillId(0)
    , m_skillAttackRange(0)
    , m_skillUseLevel(0)
    , m_gSkillDx(0)
    , m_gSkillDy(0)
    , m_preengageXOfMove(-1)
    , m_preengageYOfMove(-1)
    , m_statusEffect(nullptr)
{
}

CPlayer::~CPlayer()
{
}

void CPlayer::ProcessPreMove()
{
}

// CGrannyPc implementation moved to Granny.cpp


// CGameActor stubs
// CPc stubs
// CPlayer stubs
// CSkill stubs
// CItem stubs
