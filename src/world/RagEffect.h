#pragma once

#include "GameActor.h"
#include "res/EzEffectRes.h"
#include "res/WorldRes.h"

#include <array>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class CTexture;
struct SkillEffectPresentation;
struct SkillDirectionalStrVariant;
struct tlvertex3d;

// Shares the effect SPR/ACT frame cache with the world projectile renderer.
bool RenderSpearProjectile(const tlvertex3d& anchor, float directionX, float directionY, float alpha);
bool RenderSoulStrikeProjectile(const tlvertex3d& anchor, float alpha, float scale, u32 tint);

enum EFFECTPRIMID {
    PP_3DCIRCLE = 0,
    PP_3DCYLINDER,
    PP_3DPARTICLEORBIT,
    PP_3DPARTICLE,
    PP_3DPARTICLEGRAVITY,
    PP_3DPARTICLESPLINE,
    PP_3DTEXTURE,
    PP_3DCROSSTEXTURE,
    PP_3DSPHERE,
    PP_3DQUADHORN,
    PP_2DCIRCLE,
    PP_2DFLASH,
    PP_2DTEXTURE,
    PP_MAPPILLAR,
    PP_MAPMAGICZONE,
    PP_MAPPARTICLE,
    PP_SUPERANGEL,
    PP_RADIALSLASH,
    PP_SLASH1,
    PP_PORTALSTACK,
    PP_CASTINGRING4,
    PP_EFFECTSPRITE,
    PP_HEAL,
    PP_PORTAL,
    PP_WIND,
    PP_WATERFALL,
    PP_CLOUD,
    PP_GROUNDTEXTURE,
    PP_GROUNDCROSS,
};

struct EffectBandState {
    bool active = false;
    int process = 0;
    float maxHeight = 0.0f;
    float rotStart = 0.0f;
    float alpha = 0.0f;
    float distance = 0.0f;
    float riseAngle = 0.0f;
    float fadeThreshold = -1.0f;
    float radius = 0.0f;
    std::array<float, 21> heights{};
    std::array<u8, 21> modes{};
};

struct EffectSegmentState {
    vector3d pos{};
    float radius = 0.0f;
    float size = 0.0f;
    float alpha = 0.0f;
    int process = 0;
    float displayAngle = 0.0f;
    float phaseDegrees = 0.0f;
    matrix transform{};
};

constexpr int kRagEffectStealthField = 784; // Hold the Line's server skill units.
constexpr int kRagEffectBloodsportMist = 871; // EF_CIRCLEPOWER5
constexpr int kRagEffectPortal = 50001;
constexpr int kRagEffectReadyPortal = 50002;
constexpr int kRagEffectWarpZone = 50003;
constexpr int kRagEffectMetallicSound = 50004;
constexpr int kRagEffectMetallicSoundHit = 50005;
constexpr int kRagEffectMetallicSoundGround = 50006;
constexpr int kRagEffectMetallicFury = 50007;
constexpr int kRagEffectPerformanceNotes = 50008;
constexpr int kRagEffectSevereRainstorm = 50009;
constexpr int kRagEffectPsychicWave = 50010;
constexpr int kRagEffectTrap = 50011;
constexpr int kRagEffectMagentaTrap = 50012;
constexpr int kRagEffectCobaltTrap = 50013;
constexpr int kRagEffectMaizeTrap = 50014;
constexpr int kRagEffectVerdureTrap = 50015;
constexpr int kRagEffectMilleniumShields = 50016; // Status-owned Holy Shield loop.
constexpr int kRagEffectGospelArea = 50017; // Caster-owned visual, kept alive by Gospel units.
constexpr int kRagEffectDevotionLink = 50019; // Links from the server's ZC_DEVOTIONLIST.
constexpr int kRagEffectEnergyCoatAura = 50020; // Status-owned dark blue smoke.
// Shared with Frost Diver's target effects in skill_presentation.lua.
constexpr int kRagEffectFrozenWater = 50025;
constexpr int kRagEffectFrozenCast = 50026;
constexpr int kRagEffectFrostbite = 50071;
constexpr int kRagEffectSphereEarth = 50076;
constexpr int kRagEffectSphereWind = 50077;
constexpr int kRagEffectSphereFire = 50078;
constexpr int kRagEffectSphereWater = 50079;
// Four independent status-owned instances; their artwork is chosen by cast level.
constexpr int kRagEffectSphereSlotFirst = 50084;
constexpr int kRagEffectSphereSlotLast = 50087;

class CRagEffect;

class CEffectPrim {
public:
    CEffectPrim();

    void Init(CRagEffect* master, EFFECTPRIMID effectPrimId, const vector3d& deltaPos);
    bool OnProcess();
    void Render(matrix* viewMatrix);

    CRagEffect* m_master;
    EFFECTPRIMID m_type;
    vector3d m_deltaPos;
    vector3d m_deltaPos2;
    u32 m_renderFlag;
    int m_stateCnt;
    int m_duration;
    int m_fadeOutCnt;
    int m_pattern;
    int m_totalTexture;
    int m_spawnCount;
    float m_alpha;
    float m_alphaSpeed;
    float m_maxAlpha;
    float m_alphaDelta;
    float m_minAlpha;
    float m_size;
    float m_sizeSpeed;
    float m_sizeAccel;
    float m_radius;
    float m_radiusSpeed;
    float m_radiusAccel;
    float m_innerSize;
    float m_outerSize;
    float m_heightSize;
    float m_heightSpeed;
    float m_heightAccel;
    float m_longitude;
    float m_longSpeed;
    float m_longAccel;
    float m_speed;
    float m_accel;
    float m_gravSpeed;
    float m_gravAccel;
    float m_emitSpeed;
    float m_arcAngle;
    float m_latitude;
    float m_outerSpeed;
    float m_outerAccel;
    float m_innerSpeed;
    float m_innerAccel;
    float m_roll;
    float m_rollSpeed;
    float m_rollAccel;
    COLORREF m_tintColor;
    vector3d m_orgPos;
    vector3d m_pos;
    vector3d m_speed3d;
    matrix m_matrix;
    std::array<float, 4> m_param;
    std::vector<CTexture*> m_texture;
    std::string m_spriteActName;
    std::string m_spriteSprName;
    int m_spriteAction;
    int m_spriteMotionBase;
    int m_curMotion;
    int m_animSpeed;
    float m_spriteFrameDelay;
    float m_spriteScale;
    bool m_spriteRepeat;
    bool m_repeatAnim;
    bool m_isDisappear;
    int m_bandController;
    std::array<EffectBandState, 4> m_bands;
    int m_numSegments;
    std::array<EffectSegmentState, 12> m_segments;
};

class CRagEffect : public CGameObject {
public:
    CRagEffect();
    ~CRagEffect() override;

    void Init(CRenderObject* master, int effectId, const vector3d& deltaPos);
    void InitAtWorldPosition(int effectId, const vector3d& position);
    void InitWorld(const C3dWorldRes::effectSrcInfo& source);
    CEffectPrim* LaunchEffectPrim(EFFECTPRIMID effectPrimId, const vector3d& deltaPos);
    void DetachFromMaster(CRenderObject* knownMaster = nullptr);
    int GetEffectType() const { return m_type; }
    int GetStateCount() const { return m_stateCnt; }
    int GetDuration() const { return m_duration; }
    void SetSkillUnitId(u32 id, u32 sourceGid = 0) { m_skillUnitId = id; m_skillUnitSourceGid = sourceGid; }
    u32 GetSkillUnitId() const { return m_skillUnitId; }
    bool IsSkillUnitFor(u32 casterGid, int effectId) const;
    bool IsSkillUnitCenterFor(u32 casterGid, int effectId, const vector3d& position) const;
    bool IsSkillUnitArea() const;
    bool IsSkillUnitAreaFor(u32 casterGid, int effectId) const;
    void TrackAreaSkillUnit(u32 unitAid);
    void RemoveSkillUnit(u32 unitAid);
    bool IsDevotionLinkFrom(u32 casterGid) const;
    bool IsDevotionLinkFor(u32 casterGid, u32 targetGid) const;
    bool IsDevotionLinkTo(const CGameActor* target) const;
    void SetDevotionTarget(u32 targetGid, u16 maxDistance);
    void SetLooping(bool looping);
    void SetFreezeStatusLoop();
    bool IsFreezeStatusLoop() const { return m_freezeStatusOwned; }
    bool IsSphereStatusEffect() const { return m_type >= kRagEffectSphereSlotFirst && m_type <= kRagEffectSphereSlotLast; }
    void SetCastDurationMs(int milliseconds);
    // Keep the caster as lifetime owner while anchoring a cast marker to its cell.
    void FixWorldPosition(const vector3d& position) { m_fixedWorldPosition = position; }
    // The caster owns cleanup while the visual follows another actor.
    void FollowCastingTarget(u32 targetGid) { m_castingTargetGid = targetGid; }
    // Scheduling offset, added to this effect's configured delay_ms.
    void SetStartDelayMs(u32 milliseconds);
    void SuppressPresentationSound();
    void Cancel();
    void RefreshBloodsportMist();

    u8 OnProcess() override;
    void OnActorDeleted(const CGameActor* actor) override;
    void Render(matrix* viewMatrix) override;
    void SendMsg(CGameObject*, int, msgparam_t, msgparam_t, msgparam_t) override;

    vector3d ResolveBasePosition() const;
    float ResolveBaseRotation() const;
    bool ResolveCullSphere(vector3d* outCenter, float* outRadius) const;

private:
    u32 m_skillUnitId = 0;
    u32 m_skillUnitSourceGid = 0;
    std::vector<u32> m_areaSkillUnitIds;
    bool m_gospelNativeLayout = false;
    bool m_cancelled = false;
    bool m_freezeStatusOwned = false;
    u32 m_startDelayMs = 0;
    u32 m_castEndTick = 0;
    u32 m_castingTargetGid = 0;
    std::optional<vector3d> m_fixedWorldPosition;
    float m_presentationOffsetY = 0.0f;
    int m_presentationHueShiftDegrees = 0;
    int m_presentationSaturationPercent = 100;
    bool m_presentationGroundPlane = false;
    float m_presentationRotationSpeed = 0.0f;
    int m_strPlaysRemaining = 1;
    bool m_presentationBottomEffect = false;
    int m_presentationBottomOrder = 0;
    bool m_presentationDrawOverActors = false;
    float m_presentationBottomOffsetPx = 4.0f;
    std::string m_pendingPresentationSound;
    std::string m_replayPresentationSound;
    std::string m_loopedPresentationSound;
    void StopPresentationSoundLoop();
    const SkillEffectPresentation* m_proceduralPresentation = nullptr;
    const SkillEffectPresentation* m_directionalPresentation = nullptr;
    int m_directionalVariant = 0;
    CTexture* m_presentationTexture = nullptr;
    vector3d m_grandCrossOrigin{};
    CTexture* m_grandCrossSparkleTexture = nullptr;
    u32 m_devotionTargetGid = 0;
    u16 m_devotionMaxDistance = 0;
    CTexture* m_sonicWaveTrailTexture = nullptr;
    vector3d m_sonicWaveStartPos{};
    vector3d m_sonicWaveEndPos{};
    vector3d m_sonicWaveDirection{};
    bool m_presentationStarted = false;
    u32 m_presentationStartTick = 0;
    bool m_bloodsportMistStatusOwned = false;
    u32 m_bloodsportMistLastNotifyTick = 0;
    std::vector<float> m_burstAngles;
    std::array<CTexture*, 27> m_castConeTextures{};
    std::array<CTexture*, 3> m_stealthFogTextures{};
    struct RecoverySparkle {
        vector3d startOffset{};
        float endHeight = 0.0f;
        float size = 0.0f;
        float sparkFrequency = 0.0f;
    };
    std::array<RecoverySparkle, 12> m_recoverySparkles{};
    std::vector<std::unique_ptr<CRagEffect>> m_componentEffects;
    enum class Handler {
        None,
        EzStr,
        BodyBurst,
        GuardSphere,
        SonicWave,
        GroundCircle,
        GrandCross,
        SpriteFlash,
        SpriteGlow,
        CastCone,
        DanceRays,
        BloodsportMist,
        EnergyCoatAura,
        StealthFieldFog,
        JumpKick,
        GroundSurface,
        DevotionLink,
        Sprite,
        Entry2,
        JobLevelUp50,
        Portal,
        ReadyPortal,
        WarpZone,
        Portal2,
        ReadyPortal2,
        WarpZone2,
        MapPillar,
        MapMagicZone,
        MapParticle,
        SuperAngel,
        RecoveryHp,
        RecoverySp,
        BeginCasting,
        HealLight,
        HealMedium,
        HealLarge,
        HideStart,
        GrimTooth,
        GrimToothAtk,
        IncAgility,
        EnchantPoison,
        EnchantPoison2,
        Blessing,
        Smoke,
        Torch,
        Sight,
        SightState,
        FireBall,
        Ruwach,
        SightAura,
        FireBoltRain,
        BlueFall,
        WeatherCloud,
    };

    class CWorldAnchor : public CRenderObject {
    public:
        explicit CWorldAnchor(const vector3d& position);
    };

    void ClearPrims();
    void LoadEzEffect(const char* fName);
    void InitEZ2STRFrame();
    bool ProcessEZ2STR();
    void SpawnGroundSurface();
    void SpawnSprite();
    void RenderAniClip(int layerIndex, const KAC_LAYER& layer, const KAC_XFORMDATA& xform, matrix* viewMatrix,
        const SkillDirectionalStrVariant* direction = nullptr, float targetX = 0.0f, float targetY = 0.0f);
    void RenderDirectionalStr(matrix* viewMatrix);
    void RenderBodyBurst(const matrix& viewMatrix);
    void RenderGuardSphere(const matrix& viewMatrix);
    void RenderSonicWave(const matrix& viewMatrix);
    void RenderGroundCircle(const matrix& viewMatrix);
    void RenderGrandCross(const matrix& viewMatrix);
    void StartBuffEffect();
    void RenderCastCone(const matrix& viewMatrix);
    void RenderDanceRays(const matrix& viewMatrix);
    void RenderStatusMist(const matrix& viewMatrix);
    void RenderStealthFieldFog(const matrix& viewMatrix);
    void RenderRecoverySparkles(const matrix& viewMatrix);
    void RenderDevotionLink(const matrix& viewMatrix);
    const CGameActor* ResolveDevotionTarget() const;
    CRenderObject* ResolveCurrentMaster() const;
    CRenderObject* ResolveActorByGid(u32 actorGid) const;
    CAbleToMakeEffect* ResolveCurrentEffectOwner() const;

    void SpawnEntry2();
    void SpawnJobLevelUp50();
    void SpawnPortal();
    void SpawnReadyPortal();
    void SpawnWarpZone();
    void SpawnPortal2();
    void SpawnReadyPortal2();
    void SpawnWarpZone2();
    void SpawnMapPillar();
    void SpawnMapMagicZone();
    void SpawnMapParticle();
    void SpawnSuperAngelVariant(int variant, int birthFrame);
    void SpawnSuperAngelBurst(int startAngle);
    void UpdateSuperAngel();
    void StartRecoverySparkles();
    void SpawnBeginCasting();
    void SpawnHealLight();
    void SpawnHealMedium();
    void SpawnHealLarge();
    void SpawnHideStart();
    void SpawnGrimTooth();
    void SpawnGrimToothAtk();
    void SpawnIncAgility();
    void SpawnEnchantPoison();
    void SpawnEnchantPoison2();
    void SpawnBlessing();
    void SpawnSmoke();
    void SpawnTorch();
    void SpawnSight();
    void SpawnSightState();
    void SpawnFireBall();
    void SpawnRuwach();
    void SpawnSightAura();
    void SpawnFireBoltRain();
    void SpawnBlueFall();
    void SpawnWeatherCloud();

    std::list<CEffectPrim*> m_primList;
    CRenderObject* m_master;
    bool m_ownsMaster;
    bool m_masterIsActor;
    u32 m_masterActorGid;
    Handler m_handler;
    int m_type;
    int m_level;
    int m_flag;
    int m_count;
    int m_stateCnt;
    int m_duration;
    int m_cEndLayer;
    int m_iCurLayer;
    int m_worldType;
    bool m_loop;
    u32 m_lastProcessTick;
    float m_tickCarryMs;
    vector3d m_cachedPos;
    vector3d m_deltaPos;
    vector3d m_targetPos;
    vector3d m_grimToothStep;
    vector3d m_grimToothRemaining;
    float m_param[4];
    float m_emitSpeed;
    float m_longitude;
    float m_tlvertX;
    float m_tlvertY;
    float m_tlvertZ;
    COLORREF m_tintColor;
    std::string m_effectName;
    CEZeffectRes* m_ezEffectRes;
    KANICLIP* m_aniClips;
    std::array<KAC_XFORMDATA, 128> m_actXformData;
    std::array<u8, 128> m_isLayerDrawn;
    std::array<int, 128> m_aiCurAniKey;
    std::array<int, 128> m_activeAniKeyFrame;
    std::array<int, 128> m_activeAniKeyAppliedState;
    std::array<u8, 128> m_activeAniKeyZeroBlend;
    bool m_hasTargetPos;
};

CRagEffect* CreateWorldRagEffect(const C3dWorldRes::effectSrcInfo& source);
