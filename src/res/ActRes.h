#pragma once
#include "Types.h"
#include "res/Res.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <tuple>

// ACT frame delays are stored in units of 25 milliseconds.
constexpr float kActDelayUnitMs = 25.0f;

// Rendering samples never change the discrete motion used for sounds or combat.
struct ActMotionSample {
    int current = 0;
    int next = 0;
    int subframe = 0;
    int subdivisions = 1;

    static ActMotionSample Pose(int motion) { return { motion, motion, 0, 1 }; }
    float Fraction() const { return static_cast<float>(subframe) / subdivisions; }
    bool operator==(const ActMotionSample& other) const {
        return std::tie(current, next, subframe, subdivisions)
            == std::tie(other.current, other.next, other.subframe, other.subdivisions);
    }
    bool operator!=(const ActMotionSample& other) const { return !(*this == other); }
    bool operator<(const ActMotionSample& other) const {
        return std::tie(current, next, subframe, subdivisions)
            < std::tie(other.current, other.next, other.subframe, other.subdivisions);
    }
};

inline ActMotionSample SampleActMotion(double progress, int count, double frameDurationMs,
    bool holdLastFrame = false, int firstMotion = 0)
{
    if (count <= 1 || !std::isfinite(progress) || progress < 0.0) {
        return ActMotionSample::Pose(firstMotion);
    }
    if (holdLastFrame && progress >= count - 1) {
        return ActMotionSample::Pose(firstMotion + count - 1);
    }
    const double wrapped = std::fmod(progress, static_cast<double>(count));
    const int current = static_cast<int>(wrapped);
    // Cache a finite set of poses with intervals no longer than a 60 Hz frame.
    const double steps = std::isfinite(frameDurationMs)
        ? std::ceil(frameDurationMs * 60.0 / 1000.0) : 1.0;
    const int subdivisions = static_cast<int>((std::max)(1.0, (std::min)(120.0, steps)));
    // Nearest sampling avoids alternating repeated/skipped poses when the
    // millisecond clock lands just below a 16.666... ms boundary.
    const int subframe = (std::min)(subdivisions - 1,
        static_cast<int>(std::round((wrapped - current) * subdivisions)));
    if (subframe == 0) {
        return ActMotionSample::Pose(firstMotion + current);
    }
    return { firstMotion + current, firstMotion + (current + 1) % count, subframe, subdivisions };
}

struct CSprClip {
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

struct CAttachPointInfo {
    int x;
    int y;
    int attr;
};

struct CMotion {
    RECT range1;
    RECT range2;
    std::vector<CSprClip> sprClips;
    int eventId;
    int attachCount;
    std::vector<CAttachPointInfo> attachInfo;
};

struct CAction {
    std::vector<CMotion> motions;
};

class CActRes : public CRes {
public:
    CActRes();
    virtual ~CActRes();

    virtual bool LoadFromBuffer(const char* fName, const unsigned char* buffer, int size) override;
    virtual bool Load(const char* fName) override;
    virtual CRes* Clone() override;
    virtual void Reset() override;

    float GetDelay(int actionIndex) const;
    int GetMotionCount(int actionIndex) const;
    // Preserve the player head pose while looping an equipment ACT's idle/sitting group.
    int GetAccessoryMotionIndex(int actionIndex, int headMotionIndex, u32 elapsedMs) const;
    ActMotionSample GetAccessoryMotionSample(int actionIndex, int headMotionIndex, u32 elapsedMs) const;
    const CMotion* GetMotion(int actionIndex, int motionIndex) const;
    const char* GetEventName(int eventIndex) const;

    std::vector<CAction> actions;
    int numMaxClipPerMotion;
    std::vector<std::string> m_events;
    std::vector<float> m_delay;
};
