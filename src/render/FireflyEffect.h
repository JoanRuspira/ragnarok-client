#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

class CTexture;
class IRenderDevice;

// Ambient screen-space fireflies, enabled with the night proof of concept.
class CFireflyEffect {
public:
    CFireflyEffect();
    ~CFireflyEffect();
    // Creates the glow texture before BeginScene and releases it on map exit.
    void SetEnabled(bool enabled);
    void Draw(IRenderDevice& renderDevice);

private:
    struct Particle {
        float x, y;
        float velocityX, velocityY;
        float depth;
        float age, lifetime;
        float phase, blinkSpeed;
        uint32_t color;
    };

    struct Vertex {
        float x, y, z, oow;
        uint32_t color, specular;
        float tu, tv;
    };

    float RandomUnit();
    void Spawn(Particle& particle, bool prewarm);
    bool m_enabled = false;
    int m_width = 0;
    int m_height = 0;
    uint32_t m_randomState = 0xB5297A4Du;
    std::chrono::steady_clock::time_point m_lastUpdate{};
    std::unique_ptr<CTexture> m_texture;
    std::vector<Particle> m_particles;
    std::vector<Vertex> m_vertices;
};
