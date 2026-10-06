#pragma once

#include <chrono>
#include <cstdint>
#include <vector>
#include "ParticleFog.h"

class IRenderDevice;

// Procedural screen-space rain and particle mist: no external assets required.
class CRainEffect {
public:
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return m_enabled; }
    // Called inside the renderer's overlay pass, before gameplay UI.
    void Draw(IRenderDevice& renderDevice);

private:
    struct Drop {
        float x;
        float y;
        float depth;
    };

    // Matches D3DFVF_TLVERTEX, shared by the legacy and modern backends.
    struct Vertex {
        float x, y, z, oow;
        uint32_t color, specular;
        float tu, tv;
    };

    float RandomUnit();
    bool m_enabled = false;
    int m_width = 0;
    int m_height = 0;
    uint32_t m_randomState = 0x6D2B79F5u;
    std::chrono::steady_clock::time_point m_lastUpdate{};
    std::vector<Drop> m_drops;
    std::vector<Vertex> m_vertices;
    CParticleFog m_fog;
};
