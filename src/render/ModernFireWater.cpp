#include "ModernFireWater.h"

#include "Renderer.h"
#include "DebugLog.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float kPi = 3.141592654f;
constexpr int kTextureSize = 256;
constexpr int kFlameSize = kTextureSize / 2;

float Saturate(float value)
{
    return (std::min)(1.0f, (std::max)(0.0f, value));
}

float Fraction(float value)
{
    return value - std::floor(value);
}

float SmoothStep(float low, float high, float value)
{
    const float t = Saturate((value - low) / (high - low));
    return t * t * (3.0f - 2.0f * t);
}

u32 Color(float alpha, float red, float green, float blue)
{
    const auto channel = [](float value) { return static_cast<u32>(Saturate(value / 255.0f) * 255.0f + 0.5f); };
    return (channel(alpha) << 24) | (channel(red) << 16) | (channel(green) << 8) | channel(blue);
}

std::unique_ptr<CTexture> Upload(int size, std::vector<unsigned int>& pixels)
{
    auto texture = std::make_unique<CTexture>();
    if (!texture->Create(size, size, PF_A8R8G8B8, false)) {
        return nullptr;
    }
    texture->Update(0, 0, size, size, pixels.data(), true, size * static_cast<int>(sizeof(unsigned int)));
    if (texture->m_updateWidth != static_cast<unsigned int>(size)
        || texture->m_updateHeight != static_cast<unsigned int>(size)) {
        return nullptr;
    }
    return texture;
}

bool Project(const CRenderer& renderer, const matrix& view, const vector3d& position, tlvertex3d& out)
{
    const float depth = position.x * view.m[0][2] + position.y * view.m[1][2]
        + position.z * view.m[2][2] + view.m[3][2];
    if (!std::isfinite(depth) || depth <= 80.0f) {
        return false;
    }
    out.oow = 1.0f / depth;
    out.x = renderer.m_xoffset + (position.x * view.m[0][0] + position.y * view.m[1][0]
        + position.z * view.m[2][0] + view.m[3][0]) * renderer.m_hpc * out.oow;
    out.y = renderer.m_yoffset + (position.x * view.m[0][1] + position.y * view.m[1][1]
        + position.z * view.m[2][1] + view.m[3][1]) * renderer.m_vpc * out.oow;
    out.z = Saturate((1500.0f / 1490.0f) * (depth - 10.0f) * out.oow);
    out.specular = 0xFF000000u;
    return std::isfinite(out.x) && std::isfinite(out.y);
}

RPFace* Quad(CRenderer& renderer, CTexture* texture)
{
    RPFace* face = renderer.BorrowNullRP();
    if (!face) {
        return nullptr;
    }
    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 0;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = 0.0f;
    return face;
}

void Billboard(CRenderer& renderer, const matrix& view, const vector3d& position,
    CTexture* texture, float width, float height, float angle, u32 color, int variant = -1)
{
    tlvertex3d anchor{};
    if (!Project(renderer, view, position, anchor)) {
        return;
    }
    const float halfWidth = width * renderer.m_hpc * anchor.oow * 0.5f;
    const float halfHeight = height * renderer.m_vpc * anchor.oow * 0.5f;
    const float margin = halfWidth + halfHeight;
    if (anchor.x + margin < 0.0f || anchor.y + margin < 0.0f
        || anchor.x - margin > renderer.m_width || anchor.y - margin > renderer.m_height) {
        return;
    }
    RPFace* face = Quad(renderer, texture);
    if (!face) {
        return;
    }
    // Straight-alpha additive blending preserves feathered edges in map fog.
    face->destAlphaMode = D3DBLEND_ONE;
    face->alphaSortKey = anchor.oow;
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    const float u0 = variant < 0 ? 0.0f : (variant & 1) * 0.5f;
    const float v0 = variant < 0 ? 0.0f : (variant >> 1) * 0.5f;
    const float span = variant < 0 ? 1.0f : 0.5f;
    // Inset the atlas UVs so bilinear sampling cannot pick up another tongue.
    const float inset = variant < 0 ? 0.0f : 0.5f / kTextureSize;
    for (int i = 0; i < 4; ++i) {
        const float x = (i & 1) ? halfWidth : -halfWidth;
        const float y = i >= 2 ? halfHeight : -halfHeight;
        auto& vertex = face->m_verts[i];
        vertex = anchor;
        vertex.x += x * cosine - y * sine;
        vertex.y += x * sine + y * cosine;
        vertex.z = (std::max)(0.0f, vertex.z - 0.00002f);
        vertex.color = color;
        vertex.tu = u0 + ((i & 1) ? span - inset : inset);
        vertex.tv = v0 + (i >= 2 ? span - inset : inset);
    }
    renderer.AddRP(face, 1 | 4);
}

} // namespace

CModernFireWater::CModernFireWater() = default;
CModernFireWater::~CModernFireWater() = default;

void CModernFireWater::SetEnabled(bool enabled)
{
    m_enabled = false;
    m_flames.reset();
    m_glow.reset();
    m_water.reset();
    m_ripples.reset();
    m_time = 0.0f;
    if (!enabled) {
        return;
    }

    // Four different tongues, with tapered, curling silhouettes and hot bases.
    std::vector<unsigned int> pixels(kTextureSize * kTextureSize);
    for (int variant = 0; variant < 4; ++variant) {
        for (int y = 0; y < kFlameSize; ++y) {
            for (int x = 0; x < kFlameSize; ++x) {
                const float u = (x + 0.5f) / kFlameSize;
                const float v = (y + 0.5f) / kFlameSize;
                const float curl = 0.13f * std::sin(v * 9.0f + variant * 1.7f) * (1.0f - v);
                const float radius = (0.025f + 0.34f * std::pow(v, 0.85f))
                    * (0.78f + 0.22f * std::sin(v * 17.0f + variant));
                const float edge = Saturate(1.0f - std::fabs(u - 0.5f - curl) / radius);
                const float taper = Saturate(v * 12.0f) * Saturate((1.0f - v) * 9.0f);
                const float hot = Saturate(v * v + edge * 0.28f);
                pixels[(y + (variant >> 1) * kFlameSize) * kTextureSize + x + (variant & 1) * kFlameSize]
                    = Color(255.0f * edge * edge * taper, 255.0f, 55.0f + 195.0f * hot, 8.0f + 155.0f * hot * hot);
            }
        }
    }
    m_flames = Upload(kTextureSize, pixels);

    constexpr int glowSize = 64;
    pixels.resize(glowSize * glowSize);
    for (int y = 0; y < glowSize; ++y) {
        for (int x = 0; x < glowSize; ++x) {
            const float dx = (x + 0.5f) * 2.0f / glowSize - 1.0f;
            const float dy = (y + 0.5f) * 2.0f / glowSize - 1.0f;
            const float edge = Saturate(1.0f - dx * dx - dy * dy);
            pixels[y * glowSize + x] = Color(255.0f * edge * edge * edge, 255.0f, 255.0f, 255.0f);
        }
    }
    m_glow = Upload(glowSize, pixels);

    // Integer-frequency waves make these textures seamless in both directions.
    pixels.resize(kTextureSize * kTextureSize);
    std::vector<unsigned int> ripples(pixels.size());
    for (int y = 0; y < kTextureSize; ++y) {
        for (int x = 0; x < kTextureSize; ++x) {
            const float u = (x + 0.5f) * 2.0f * kPi / kTextureSize;
            const float v = (y + 0.5f) * 2.0f * kPi / kTextureSize;
            const float warp = u + 0.40f * std::sin(v * 2.0f) + 0.18f * std::sin(u * 3.0f - v);
            const float waves = std::sin(warp * 3.0f + v * 2.0f)
                + 0.30f * std::sin(v * 4.0f - u + 0.50f * std::sin(warp * 2.0f));
            // Broad color bands and thicker, nearly white crests give the
            // flowing layers a painted, cartoony look rather than fine glints.
            const float shade = 0.68f + 0.17f * SmoothStep(-0.40f, -0.30f, waves)
                + 0.15f * SmoothStep(0.22f, 0.32f, waves);
            pixels[y * kTextureSize + x] = Color(255.0f, shade * 190.0f, shade * 230.0f, shade * 255.0f);
            const float crest = 1.0f - SmoothStep(0.10f, 0.42f, std::fabs(waves - 0.35f));
            const float broken = 0.72f + 0.28f * std::sin(u * 3.0f + v * 5.0f) * std::sin(u * 3.0f + v * 5.0f);
            ripples[y * kTextureSize + x] = Color(255.0f * crest * broken, 236.0f, 251.0f, 255.0f);
        }
    }
    m_water = Upload(kTextureSize, pixels);
    m_ripples = Upload(kTextureSize, ripples);
    if (!m_flames || !m_glow || !m_water || !m_ripples) {
        DbgLog("[Effects] Modern fire/water texture creation failed; using classic effects\n");
        SetEnabled(false);
        return;
    }
    m_start = std::chrono::steady_clock::now();
    m_enabled = true;
}

void CModernFireWater::BeginFrame()
{
    if (m_enabled) {
        m_time = static_cast<float>(std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count());
    }
}

float CModernFireWater::WaterHeight(float x, float z, float amplitude) const
{
    return amplitude * (0.62f * std::sin(x * 0.100f + z * 0.060f + m_time * 2.2f)
        + 0.28f * std::sin(-x * 0.065f + z * 0.140f - m_time * 1.5f)
        + 0.10f * std::sin(x * 0.045f - z * 0.055f + m_time * 0.8f));
}

void CModernFireWater::QueueTorch(CRenderer& renderer, const matrix& view, const vector3d& position, float scale)
{
    if (!m_enabled) {
        return;
    }
    scale = std::isfinite(scale) && scale > 0.0f ? (std::min)(4.0f, scale) : 1.0f;
    const float phase = position.x * 0.113f + position.z * 0.077f;
    const float flicker = 0.88f + 0.08f * std::sin(m_time * 13.0f + phase)
        + 0.04f * std::sin(m_time * 21.0f - phase);
    const vector3d base{ position.x, position.y - 5.5f * scale, position.z };
    const vector3d glow{ base.x, base.y - 3.0f * scale, base.z };
    Billboard(renderer, view, glow, m_glow.get(), 28.0f * scale, 24.0f * scale, 0.0f,
        Color(65.0f * flicker, 255.0f, 105.0f, 25.0f));

    // The larger outer tongue and smaller hot core breathe independently.
    for (int i = 0; i < 3; ++i) {
        const float sway = std::sin(m_time * (4.5f + i) + phase + i * 2.0f);
        const float height = (12.0f - i * 2.3f) * scale * (flicker + 0.08f * sway);
        const vector3d center{ base.x + sway * 0.65f * scale, base.y - height * 0.42f, base.z };
        Billboard(renderer, view, center, m_flames.get(), (7.0f - i * 1.4f) * scale,
            height, sway * 0.12f, Color(140.0f + i * 25.0f, 255.0f, 255.0f, 255.0f), i);
    }

    // Analytic particle ages keep emission smooth at any frame rate, without
    // allocating or retaining per-emitter particle lists.
    for (int i = 0; i < 7; ++i) {
        const float age = Fraction(m_time / (0.85f + i * 0.065f) + phase * 0.07f + i * 0.618f);
        const float swirl = phase + i * 2.4f + age * 5.0f;
        const vector3d center{ base.x + std::sin(swirl) * age * 2.8f * scale,
            base.y - (2.0f + age * 15.0f) * scale,
            base.z + std::cos(swirl) * age * 1.2f * scale };
        const float fade = std::sin(age * kPi) * (1.0f - age);
        Billboard(renderer, view, center, m_flames.get(), (4.0f - age * 2.5f) * scale,
            (5.0f + age * 3.0f) * scale, std::sin(swirl) * 0.25f,
            Color(120.0f * fade, 255.0f, 235.0f, 210.0f), i & 3);
    }
    for (int i = 0; i < 10; ++i) {
        const float age = Fraction(m_time / (1.4f + i * 0.13f) + phase * 0.11f + i * 0.754f);
        const float swirl = phase + i * 2.7f;
        const vector3d center{ base.x + (std::sin(swirl) * 2.0f + std::sin(age * 6.0f + swirl) * age * 4.0f) * scale,
            base.y - (4.0f + age * (18.0f + i)) * scale,
            base.z + std::cos(swirl + age * 3.0f) * age * 3.0f * scale };
        Billboard(renderer, view, center, m_glow.get(), (0.45f + (i % 3) * 0.12f) * scale,
            (0.9f + age) * scale, std::sin(swirl) * 0.3f,
            Color(240.0f * std::sin(age * kPi) * (1.0f - age), 255.0f, 175.0f - age * 75.0f, 45.0f));
    }
}

void CModernFireWater::QueueWater(CRenderer& renderer, const matrix& view,
    const vector3d (&positions)[4], const float (&depths)[4], u32 mapTint)
{
    if (!m_enabled) {
        return;
    }
    tlvertex3d projected[4]{};
    for (int i = 0; i < 4; ++i) {
        if (!Project(renderer, view, positions[i], projected[i])) {
            return;
        }
    }
    const float tintR = ((mapTint >> 16) & 255u) / 255.0f;
    const float tintG = ((mapTint >> 8) & 255u) / 255.0f;
    const float tintB = (mapTint & 255u) / 255.0f;
    const float flowU = Fraction(m_time * 0.034f);
    const float flowV = Fraction(m_time * 0.022f);
    for (int layer = 0; layer < 2; ++layer) {
        RPFace* face = Quad(renderer, layer == 0 ? m_water.get() : m_ripples.get());
        if (!face) {
            return;
        }
        face->alphaSortKey = projected[0].oow;
        for (int i = 0; i < 4; ++i) {
            const auto& position = positions[i];
            auto& vertex = face->m_verts[i];
            vertex = projected[i];
            const float deep = Saturate(depths[i] / 9.0f);
            const float wet = Saturate(depths[i] * 2.0f);
            const float dx = renderer.m_eyePos.x - position.x;
            const float dy = renderer.m_eyePos.y - position.y;
            const float dz = renderer.m_eyePos.z - position.z;
            const float distance = (std::max)(0.001f, std::sqrt(dx * dx + dy * dy + dz * dz));
            const float grazing = 1.0f - Saturate(std::fabs(dy) / distance);
            const float reflection = grazing * grazing * grazing;
            if (layer == 0) {
                vertex.color = Color(wet * (185.0f + deep * 30.0f),
                    (63.0f - deep * 25.0f + reflection * 30.0f) * tintR,
                    (165.0f - deep * 50.0f + reflection * 30.0f) * tintG,
                    (223.0f - deep * 40.0f + reflection * 30.0f) * tintB);
                const float wobble = 0.025f * std::sin(position.x * 0.10f + position.z * 0.06f + m_time * 2.2f);
                vertex.tu = position.x * 0.0105f + flowU + wobble;
                vertex.tv = position.z * 0.0105f + flowV + wobble * 0.5f;
            } else {
                const float shore = Saturate(1.0f - depths[i] / 4.0f);
                const float glint = 0.80f + 0.20f * std::sin(position.x * 0.18f + position.z * 0.13f - m_time * 2.0f);
                vertex.color = Color(wet * (75.0f + reflection * 65.0f + shore * shore * 110.0f) * glint,
                    239.0f * tintR, 251.0f * tintG, 255.0f * tintB);
                // A rotated, counterflowing layer breaks up repeated wave lines.
                vertex.tu = (position.x * 0.80f + position.z * 0.60f) * 0.021f - flowV;
                vertex.tv = (-position.x * 0.60f + position.z * 0.80f) * 0.021f + Fraction(m_time * 0.0238f);
                vertex.z = (std::max)(0.0f, vertex.z - 0.00001f);
            }
        }
        renderer.AddRP(face, 1);
    }
}
