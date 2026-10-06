#pragma once

#include <cstdint>
#include <memory>
#include <vector>

class CTexture;
class IRenderDevice;

// Animated screen-space mist, using a procedurally generated texture atlas.
class CParticleFog {
public:
    CParticleFog();
    ~CParticleFog();
    void Initialize();
    void Reset();
    // Uses the rain overlay's alpha blending and disabled depth/fog states.
    void Draw(IRenderDevice& renderDevice, float deltaSeconds);

private:
    struct Particle {
        float x, y;
        float velocityX, velocityY;
        float depth;
        float age, lifetime;
        float size, rotation, phase;
        unsigned int variant;
    };

    struct Vertex {
        float x, y, z, oow;
        uint32_t color, specular;
        float tu, tv;
    };

    float RandomUnit();
    void Spawn(Particle& particle, bool prewarm);
    int m_width = 0;
    int m_height = 0;
    uint32_t m_randomState = 0xA341316Cu;
    std::unique_ptr<CTexture> m_texture;
    std::vector<Particle> m_particles;
    std::vector<Vertex> m_vertices;
};
