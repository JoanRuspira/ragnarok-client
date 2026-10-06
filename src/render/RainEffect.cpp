#include "RainEffect.h"
#include "render3d/RenderDevice.h"

#include <algorithm>

namespace {

constexpr float kSlant = 0.18f;
constexpr float kMargin = 64.0f;
constexpr uint32_t kRainColor = 0x00BCD8F0u;
constexpr int kMaxDrops = 1500;

} // namespace

void CRainEffect::SetEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_width = 0;
    m_height = 0;
    m_drops.clear();
    m_vertices.clear();
    m_lastUpdate = {};
    m_fog.Reset();
    if (enabled) {
        m_fog.Initialize();
    }
}

float CRainEffect::RandomUnit()
{
    // Keep weather randomness separate from gameplay's random sequence.
    m_randomState ^= m_randomState << 13;
    m_randomState ^= m_randomState >> 17;
    m_randomState ^= m_randomState << 5;
    return static_cast<float>(m_randomState >> 8) / 16777216.0f;
}

void CRainEffect::Draw(IRenderDevice& renderDevice)
{
    const int width = renderDevice.GetRenderWidth();
    const int height = renderDevice.GetRenderHeight();
    if (!m_enabled || width <= 0 || height <= 0) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    float deltaSeconds = 0.0f;
    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        // Scale density to the viewport, with a fixed upper bound on draw work.
        const double count = static_cast<double>(width) * height / 1750.0;
        const int dropCount = static_cast<int>((std::max)(100.0, (std::min)(static_cast<double>(kMaxDrops), count)));
        m_drops.resize(static_cast<size_t>(dropCount));
        m_vertices.reserve(m_drops.size() * 6u);
        for (Drop& drop : m_drops) {
            drop.x = RandomUnit() * (width + 2.0f * kMargin) - kMargin;
            drop.y = RandomUnit() * (height + 2.0f * kMargin) - kMargin;
            drop.depth = RandomUnit();
        }
    } else {
        // Avoid a jump when rendering resumes after a stall or minimization.
        deltaSeconds = (std::min)(0.1f, std::chrono::duration<float>(now - m_lastUpdate).count());
    }
    m_lastUpdate = now;

    const float scale = (std::max)(0.75f, (std::min)(1.5f, height / 720.0f));
    m_vertices.clear();
    for (Drop& drop : m_drops) {
        const float fall = (360.0f + 360.0f * drop.depth) * scale * deltaSeconds;
        drop.y += fall;
        drop.x += fall * kSlant;
        if (drop.y > height + kMargin) {
            drop.y -= height + 2.0f * kMargin;
            drop.x = RandomUnit() * (width + 2.0f * kMargin) - kMargin;
        }
        if (drop.x > width + kMargin) {
            drop.x -= width + 2.0f * kMargin;
        }

        const float length = (12.0f + 24.0f * drop.depth) * scale;
        const float halfWidth = (0.35f + 0.45f * drop.depth) * scale;
        const float tailX = drop.x - length * kSlant;
        const float tailY = drop.y - length;
        const uint32_t headColor = (static_cast<uint32_t>(55.0f + 75.0f * drop.depth) << 24) | kRainColor;
        // Fade the tail to transparent using vertex alpha, without a texture.
        const Vertex tailLeft{ tailX - halfWidth, tailY, 0.0f, 1.0f, kRainColor, 0u, 0.0f, 0.0f };
        const Vertex tailRight{ tailX + halfWidth, tailY, 0.0f, 1.0f, kRainColor, 0u, 0.0f, 0.0f };
        const Vertex headLeft{ drop.x - halfWidth, drop.y, 0.0f, 1.0f, headColor, 0u, 0.0f, 0.0f };
        const Vertex headRight{ drop.x + halfWidth, drop.y, 0.0f, 1.0f, headColor, 0u, 0.0f, 0.0f };
        m_vertices.insert(m_vertices.end(), { tailLeft, tailRight, headLeft, headLeft, tailRight, headRight });
    }

    renderDevice.BindTexture(0, nullptr);
    renderDevice.BindTexture(1, nullptr);
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
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
    renderDevice.SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
    // Mist drifts behind the original rain streaks, with the UI above both.
    m_fog.Draw(renderDevice, deltaSeconds);
    renderDevice.BindTexture(0, nullptr);
    renderDevice.SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    renderDevice.SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    renderDevice.DrawPrimitive(D3DPT_TRIANGLELIST, D3DFVF_TLVERTEX,
        m_vertices.data(), static_cast<DWORD>(m_vertices.size()), 0);
}
