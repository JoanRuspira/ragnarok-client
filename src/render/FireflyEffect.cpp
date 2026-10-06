#include "FireflyEffect.h"

#include "render3d/RenderDevice.h"
#include "res/Texture.h"
#include "DebugLog.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kGlowSize = 64;
constexpr int kMaxFireflies = 96;
constexpr float kPi = 3.141592654f;
constexpr float kMargin = 32.0f;

} // namespace

CFireflyEffect::CFireflyEffect() = default;
CFireflyEffect::~CFireflyEffect() = default;

void CFireflyEffect::SetEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_width = 0;
    m_height = 0;
    m_lastUpdate = {};
    m_particles.clear();
    m_vertices.clear();
    m_texture.reset();
    if (!enabled) {
        return;
    }

    std::vector<unsigned int> pixels(kGlowSize * kGlowSize);
    for (int y = 0; y < kGlowSize; ++y) {
        for (int x = 0; x < kGlowSize; ++x) {
            const float dx = (x + 0.5f) * 2.0f / kGlowSize - 1.0f;
            const float dy = (y + 0.5f) * 2.0f / kGlowSize - 1.0f;
            const float edge = (std::max)(0.0f, 1.0f - dx * dx - dy * dy);
            const uint32_t alpha = static_cast<uint32_t>(255.0f * edge * edge * edge + 0.5f);
            pixels[y * kGlowSize + x] = (alpha << 24) | 0x00FFFFFFu;
        }
    }
    auto texture = std::make_unique<CTexture>();
    if (!texture->Create(kGlowSize, kGlowSize, PF_A8R8G8B8, false)) {
        DbgLog("[Weather] Could not create firefly glow texture\n");
        return;
    }
    // Upload outside the scene so Vulkan can schedule the texture transfer.
    texture->Update(0, 0, kGlowSize, kGlowSize, pixels.data(), true,
        kGlowSize * static_cast<int>(sizeof(unsigned int)));
    if (texture->m_updateWidth != kGlowSize || texture->m_updateHeight != kGlowSize) {
        DbgLog("[Weather] Could not upload firefly glow texture\n");
        return;
    }
    m_texture = std::move(texture);
}

float CFireflyEffect::RandomUnit()
{
    // Keep this sequence independent of rain, mist and gameplay randomness.
    m_randomState ^= m_randomState << 13;
    m_randomState ^= m_randomState >> 17;
    m_randomState ^= m_randomState << 5;
    return static_cast<float>(m_randomState >> 8) / 16777216.0f;
}

void CFireflyEffect::Spawn(Particle& particle, bool prewarm)
{
    const float scale = (std::max)(0.75f, (std::min)(2.0f, m_height / 720.0f));
    const float margin = kMargin * scale;
    particle.x = RandomUnit() * (m_width + 2.0f * margin) - margin;
    particle.y = RandomUnit() * (m_height + 2.0f * margin) - margin;
    particle.velocityX = (RandomUnit() - 0.5f) * 18.0f;
    particle.velocityY = (RandomUnit() - 0.5f) * 12.0f;
    particle.depth = RandomUnit();
    particle.lifetime = 20.0f + RandomUnit() * 20.0f;
    particle.age = prewarm ? RandomUnit() * particle.lifetime : 0.0f;
    particle.phase = RandomUnit() * 2.0f * kPi;
    particle.blinkSpeed = 1.2f + RandomUnit() * 1.4f;
    particle.color = RandomUnit() < 0.55f ? 0x00FFE7A1u : 0x00D3FF95u;
}

void CFireflyEffect::Draw(IRenderDevice& renderDevice)
{
    const int width = renderDevice.GetRenderWidth();
    const int height = renderDevice.GetRenderHeight();
    if (!m_enabled || !m_texture || width <= 0 || height <= 0) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    float deltaSeconds = 0.0f;
    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        const double count = static_cast<double>(width) * height / 24000.0;
        const int particleCount = static_cast<int>((std::max)(20.0,
            (std::min)(static_cast<double>(kMaxFireflies), count)));
        m_particles.resize(static_cast<size_t>(particleCount));
        m_vertices.reserve(m_particles.size() * 12u);
        for (Particle& particle : m_particles) {
            Spawn(particle, true);
        }
    } else {
        deltaSeconds = (std::max)(0.0f, (std::min)(0.1f,
            std::chrono::duration<float>(now - m_lastUpdate).count()));
    }
    m_lastUpdate = now;

    const float scale = (std::max)(0.75f, (std::min)(2.0f, height / 720.0f));
    const float margin = kMargin * scale;
    const float maxU = static_cast<float>(kGlowSize) / m_texture->m_w;
    const float maxV = static_cast<float>(kGlowSize) / m_texture->m_h;
    m_vertices.clear();
    for (Particle& particle : m_particles) {
        particle.age += deltaSeconds;
        if (particle.age >= particle.lifetime) {
            Spawn(particle, false);
        }
        const float speed = (0.55f + particle.depth * 0.75f) * scale;
        const float flutter = particle.age + particle.phase;
        particle.x += (particle.velocityX + 9.0f * std::sin(flutter * 0.75f)
            + 3.0f * std::sin(flutter * 2.1f)) * speed * deltaSeconds;
        particle.y += (particle.velocityY + 6.0f * std::cos(flutter * 0.6f)
            + 2.0f * std::sin(flutter * 1.7f)) * speed * deltaSeconds;
        // Wrap beyond the entire halo so it never jumps while visible.
        if (particle.x < -margin) particle.x += width + 2.0f * margin;
        else if (particle.x > width + margin) particle.x -= width + 2.0f * margin;
        if (particle.y < -margin) particle.y += height + 2.0f * margin;
        else if (particle.y > height + margin) particle.y -= height + 2.0f * margin;

        const float lifeFade = std::sin(kPi * particle.age / particle.lifetime);
        const float blink = 0.5f + 0.5f * std::sin(particle.age * particle.blinkSpeed + particle.phase);
        const float glow = lifeFade * lifeFade * (0.08f + 0.92f * blink * blink * blink);
        const float depthGlow = 0.6f + particle.depth * 0.4f;
        auto addQuad = [&](float diameter, uint32_t rgb, float opacity) {
            const float radius = diameter * scale * 0.5f;
            const uint32_t color = (static_cast<uint32_t>(opacity * 255.0f + 0.5f) << 24) | rgb;
            const Vertex topLeft{ particle.x - radius, particle.y - radius, 0.0f, 1.0f, color, 0u, 0.0f, 0.0f };
            const Vertex topRight{ particle.x + radius, particle.y - radius, 0.0f, 1.0f, color, 0u, maxU, 0.0f };
            const Vertex bottomLeft{ particle.x - radius, particle.y + radius, 0.0f, 1.0f, color, 0u, 0.0f, maxV };
            const Vertex bottomRight{ particle.x + radius, particle.y + radius, 0.0f, 1.0f, color, 0u, maxU, maxV };
            m_vertices.insert(m_vertices.end(), { topLeft, topRight, bottomLeft, bottomLeft, topRight, bottomRight });
        };
        addQuad(14.0f + particle.depth * 12.0f, particle.color, glow * depthGlow * 0.45f);
        addQuad(2.2f + particle.depth * 1.6f, 0x00FFFDE2u, glow * depthGlow);
    }

    renderDevice.BindTexture(0, m_texture.get());
    renderDevice.BindTexture(1, nullptr);
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    renderDevice.SetTextureStageState(0, D3DTSS_MINFILTER, D3DTFN_LINEAR);
    renderDevice.SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTFG_LINEAR);
    renderDevice.SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    renderDevice.SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    renderDevice.SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);
    renderDevice.SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
    renderDevice.SetRenderState(D3DRENDERSTATE_ALPHATESTENABLE, FALSE);
    renderDevice.SetRenderState(D3DRENDERSTATE_COLORKEYENABLE, FALSE);
    renderDevice.SetRenderState(D3DRENDERSTATE_FOGENABLE, FALSE);
    renderDevice.SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
    renderDevice.SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE);
    renderDevice.SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
    renderDevice.SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_ONE);
    renderDevice.DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX,
        m_vertices.data(), static_cast<DWORD>(m_vertices.size()), 0);
    renderDevice.BindTexture(0, nullptr);
}
