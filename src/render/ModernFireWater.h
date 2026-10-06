#pragma once

#include "Types.h"

#include <chrono>
#include <memory>

class CRenderer;
class CTexture;

// Shared, procedural replacements for map torches and ground water.
class CModernFireWater {
public:
    CModernFireWater();
    ~CModernFireWater();

    // Creates textures outside BeginScene; disabling releases them on map exit.
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }
    void BeginFrame();
    float WaterHeight(float x, float z, float amplitude) const;
    void QueueTorch(CRenderer& renderer, const matrix& view, const vector3d& position, float scale);
    void QueueWater(CRenderer& renderer, const matrix& view,
        const vector3d (&positions)[4], const float (&depths)[4], u32 mapTint);

private:
    bool m_enabled = false;
    float m_time = 0.0f;
    std::chrono::steady_clock::time_point m_start{};
    std::unique_ptr<CTexture> m_flames;
    std::unique_ptr<CTexture> m_glow;
    std::unique_ptr<CTexture> m_water;
    std::unique_ptr<CTexture> m_ripples;
};
