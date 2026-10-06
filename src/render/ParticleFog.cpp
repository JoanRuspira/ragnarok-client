#include "ParticleFog.h"
#include "render3d/RenderDevice.h"
#include "res/Texture.h"
#include "DebugLog.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kPatchSize = 128;
constexpr int kAtlasSize = kPatchSize * 2;
constexpr float kPi = 3.141592654f;
constexpr uint32_t kMistColor = 0x008CB8ECu;

float NoiseCorner(int x, int y, uint32_t seed)
{
    uint32_t hash = static_cast<uint32_t>(x) * 0x8DA6B343u
        ^ static_cast<uint32_t>(y) * 0xD8163841u ^ seed;
    hash ^= hash >> 16;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15;
    return static_cast<float>(hash >> 8) / 16777216.0f;
}

float SmoothNoise(float x, float y, uint32_t seed)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    float tx = x - ix;
    float ty = y - iy;
    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    const float top = NoiseCorner(ix, iy, seed) * (1.0f - tx) + NoiseCorner(ix + 1, iy, seed) * tx;
    const float bottom = NoiseCorner(ix, iy + 1, seed) * (1.0f - tx) + NoiseCorner(ix + 1, iy + 1, seed) * tx;
    return top * (1.0f - ty) + bottom * ty;
}

} // namespace

CParticleFog::CParticleFog() = default;
CParticleFog::~CParticleFog() = default;

void CParticleFog::Initialize()
{
    if (m_texture) {
        return;
    }

    // Four irregular, feathered patches keep overlapping particles from
    // repeating the same silhouette. The atlas is uploaded once per map.
    std::vector<unsigned int> pixels(static_cast<size_t>(kAtlasSize) * kAtlasSize);
    for (unsigned int variant = 0; variant < 4; ++variant) {
        const uint32_t seed = 0x9E3779B9u * (variant + 1u);
        const int originX = static_cast<int>(variant & 1u) * kPatchSize;
        const int originY = static_cast<int>(variant >> 1u) * kPatchSize;
        for (int y = 0; y < kPatchSize; ++y) {
            for (int x = 0; x < kPatchSize; ++x) {
                const float u = (x + 0.5f) / kPatchSize;
                const float v = (y + 0.5f) / kPatchSize;
                const float dx = u * 2.0f - 1.0f;
                const float dy = v * 2.0f - 1.0f;
                const float edge = (std::max)(0.0f, 1.0f - dx * dx - dy * dy);
                const float noise = 0.55f * SmoothNoise(u * 4.0f, v * 4.0f, seed)
                    + 0.30f * SmoothNoise(u * 9.0f, v * 9.0f, seed + 1u)
                    + 0.15f * SmoothNoise(u * 18.0f, v * 18.0f, seed + 2u);
                const float wisps = (std::max)(0.0f, (noise - 0.2f) * 1.5f);
                const float alpha = (std::min)(1.0f, edge * edge * wisps);
                const unsigned int alphaByte = static_cast<unsigned int>(alpha * 255.0f + 0.5f);
                pixels[static_cast<size_t>(originY + y) * kAtlasSize + originX + x]
                    = (alphaByte << 24) | 0x00FFFFFFu;
            }
        }
    }

    auto texture = std::make_unique<CTexture>();
    if (!texture->Create(kAtlasSize, kAtlasSize, PF_A8R8G8B8, false)) {
        DbgLog("[Weather] Could not create particle fog texture\n");
        return;
    }
    // Upload before BeginScene, so Vulkan can schedule the texture transfer.
    texture->Update(0, 0, kAtlasSize, kAtlasSize, pixels.data(), true,
        kAtlasSize * static_cast<int>(sizeof(unsigned int)));
    m_texture = std::move(texture);
}

void CParticleFog::Reset()
{
    m_texture.reset();
    m_particles.clear();
    m_vertices.clear();
    m_width = 0;
    m_height = 0;
}

float CParticleFog::RandomUnit()
{
    // Mist and rain have independent sequences, keeping the restored rain intact.
    m_randomState ^= m_randomState << 13;
    m_randomState ^= m_randomState >> 17;
    m_randomState ^= m_randomState << 5;
    return static_cast<float>(m_randomState >> 8) / 16777216.0f;
}

void CParticleFog::Spawn(Particle& particle, bool prewarm)
{
    particle.x = (RandomUnit() * 1.5f - 0.25f) * m_width;
    particle.y = RandomUnit() * m_height;
    particle.depth = RandomUnit();
    const float direction = RandomUnit() * kPi * 2.0f;
    const float speed = 38.0f + 30.0f * particle.depth;
    particle.velocityX = std::cos(direction) * speed;
    particle.velocityY = std::sin(direction) * speed * 0.55f;
    particle.lifetime = 24.0f + RandomUnit() * 20.0f;
    particle.age = prewarm ? RandomUnit() * particle.lifetime : 0.0f;
    particle.size = 0.85f + RandomUnit() * 0.45f;
    particle.rotation = (RandomUnit() - 0.5f) * 0.35f;
    particle.phase = RandomUnit() * kPi * 2.0f;
    particle.variant = static_cast<unsigned int>(RandomUnit() * 4.0f);
}

void CParticleFog::Draw(IRenderDevice& renderDevice, float deltaSeconds)
{
    if (!m_texture) {
        return;
    }
    const int width = renderDevice.GetRenderWidth();
    const int height = renderDevice.GetRenderHeight();
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        // Particle sizes scale with height; adapt the count to aspect ratio
        // so larger windows retain similar coverage without extra overdraw.
        const int count = static_cast<int>((std::max)(24.0f, (std::min)(72.0f, 24.0f * width / height)));
        m_particles.resize(static_cast<size_t>(count));
        m_vertices.reserve(m_particles.size() * 6u);
        for (Particle& particle : m_particles) {
            Spawn(particle, true);
        }
        deltaSeconds = 0.0f;
    }

    m_vertices.clear();
    const float scale = height / 720.0f;
    const float patchU = static_cast<float>(kPatchSize) / m_texture->m_w;
    const float patchV = static_cast<float>(kPatchSize) / m_texture->m_h;
    for (Particle& particle : m_particles) {
        particle.age += deltaSeconds;
        if (particle.age >= particle.lifetime) {
            Spawn(particle, false);
        }
        // Individual drift directions and gentle eddies circulate the layers.
        particle.x += (particle.velocityX + 12.0f * std::sin(particle.age * 0.45f + particle.phase))
            * scale * deltaSeconds;
        particle.y += (particle.velocityY + 18.0f * std::cos(particle.age * 0.35f + particle.phase))
            * scale * deltaSeconds;

        // Smooth births/deaths prevent clouds popping or obvious screen wrapping.
        const float fade = std::sin(kPi * particle.age / particle.lifetime);
        const float opacity = (0.38f + 0.16f * particle.depth) * fade * fade;
        const uint32_t color = (static_cast<uint32_t>(opacity * 255.0f + 0.5f) << 24) | kMistColor;
        const float breathe = 1.0f + 0.08f * std::sin(particle.age * 0.25f + particle.phase);
        const float halfWidth = height * (0.34f + 0.24f * particle.depth) * particle.size * breathe;
        const float halfHeight = height * (0.13f + 0.11f * particle.depth) * particle.size * breathe;
        const float angle = particle.rotation + 0.06f * std::sin(particle.age * 0.2f + particle.phase);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        // Wrap after the entire rotated patch leaves the screen, so its
        // feathered edges enter from the opposite side without a visible jump.
        const float boundX = std::fabs(cosine) * halfWidth + std::fabs(sine) * halfHeight;
        const float boundY = std::fabs(sine) * halfWidth + std::fabs(cosine) * halfHeight;
        if (particle.x < -boundX) {
            particle.x += width + 2.0f * boundX;
        } else if (particle.x > width + boundX) {
            particle.x -= width + 2.0f * boundX;
        }
        if (particle.y < -boundY) {
            particle.y += height + 2.0f * boundY;
        } else if (particle.y > height + boundY) {
            particle.y -= height + 2.0f * boundY;
        }
        const float u = (particle.variant & 1u) * patchU;
        const float v = (particle.variant >> 1u) * patchV;
        auto corner = [&](float x, float y, float tu, float tv) {
            return Vertex{ particle.x + x * cosine - y * sine, particle.y + x * sine + y * cosine,
                0.0f, 1.0f, color, 0u, tu, tv };
        };
        const Vertex topLeft = corner(-halfWidth, -halfHeight, u, v);
        const Vertex topRight = corner(halfWidth, -halfHeight, u + patchU, v);
        const Vertex bottomLeft = corner(-halfWidth, halfHeight, u, v + patchV);
        const Vertex bottomRight = corner(halfWidth, halfHeight, u + patchU, v + patchV);
        m_vertices.insert(m_vertices.end(), { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight });
    }

    renderDevice.BindTexture(0, m_texture.get());
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    renderDevice.SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
    renderDevice.SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
    renderDevice.DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX,
        m_vertices.data(), static_cast<DWORD>(m_vertices.size()), 0);
}
