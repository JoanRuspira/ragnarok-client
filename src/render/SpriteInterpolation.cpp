#include "SpriteInterpolation.h"
#include "DC.h"
#include "res/ActRes.h"
#include "res/Sprite.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <vector>

namespace {

constexpr int kFlowGrid = 4;
constexpr int kMaxFlowDimension = 256;
constexpr size_t kFlowBudget = 16u * 1024u * 1024u;
constexpr size_t kMaxFlowPairs = 512;

struct Pixel {
    float a = 0, r = 0, g = 0, b = 0;
    Pixel operator+(const Pixel& p) const { return { a + p.a, r + p.r, g + p.g, b + p.b }; }
    Pixel operator*(float weight) const { return { a * weight, r * weight, g * weight, b * weight }; }
};

Pixel ReadPixel(const SprImg& image, int bank, const unsigned int* palette, int x, int y)
{
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) {
        return {};
    }
    const size_t offset = static_cast<size_t>(y) * image.width + x;
    unsigned int argb = 0;
    if (bank == 0) {
        if (offset >= image.indices.size() || image.indices[offset] == 0) {
            return {};
        }
        argb = 0xFF000000u | (palette[image.indices[offset]] & 0xFFFFFFu);
    } else {
        if (offset >= image.rgba.size()) {
            return {};
        }
        argb = image.rgba[offset];
    }
    const float a = (argb >> 24) / 255.0f;
    return { a, ((argb >> 16) & 255) / 255.0f * a,
        ((argb >> 8) & 255) / 255.0f * a, (argb & 255) / 255.0f * a };
}

Pixel SamplePixel(const SprImg& image, int bank, const unsigned int* palette,
    float x, float y, bool sharpPixels, float sharpness)
{
    const int ix = static_cast<int>(std::floor(x));
    const int iy = static_cast<int>(std::floor(y));
    float fx = x - ix, fy = y - iy;
    if (sharpPixels) {
        // Keep each authored pixel flat across most of its footprint. A narrow
        // transition still provides subpixel coverage for moving/rotating edges.
        // Keep the transition centered as its width changes. At the default gain
        // of 4 (or higher), a static source pixel becomes four identical 2x texels.
        auto coverage = [sharpness](float fraction) {
            return (std::max)(0.0f, (std::min)(1.0f, (fraction - 0.5f) * sharpness + 0.5f));
        };
        fx = coverage(fx);
        fy = coverage(fy);
    }
    if ((fx == 0 || fx == 1) && (fy == 0 || fy == 1)) {
        // Most sharp samples land on a pixel plateau. Avoid reading and blending
        // four neighbors when only one contributes, especially at original poses.
        return ReadPixel(image, bank, palette, ix + (fx == 1), iy + (fy == 1));
    }
    return ReadPixel(image, bank, palette, ix, iy) * ((1 - fx) * (1 - fy))
        + ReadPixel(image, bank, palette, ix + 1, iy) * (fx * (1 - fy))
        + ReadPixel(image, bank, palette, ix, iy + 1) * ((1 - fx) * fy)
        + ReadPixel(image, bank, palette, ix + 1, iy + 1) * (fx * fy);
}

struct FlowVector {
    float x = 0, y = 0, confidence = 0;
};

struct FlowField {
    int width = 0, height = 0;
    std::vector<FlowVector> vectors;

    FlowVector Sample(float x, float y) const {
        x = (std::max)(0.0f, (std::min)(static_cast<float>(width - 1), (x - 2) / kFlowGrid));
        y = (std::max)(0.0f, (std::min)(static_cast<float>(height - 1), (y - 2) / kFlowGrid));
        const int ix = static_cast<int>(x), iy = static_cast<int>(y);
        const float fx = x - ix, fy = y - iy;
        FlowVector result{};
        for (int dy = 0; dy <= 1; ++dy) {
            for (int dx = 0; dx <= 1; ++dx) {
                const auto& v = vectors[static_cast<size_t>((std::min)(height - 1, iy + dy)) * width
                    + (std::min)(width - 1, ix + dx)];
                const float weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
                result.x += v.x * weight;
                result.y += v.y * weight;
                result.confidence += v.confidence * weight;
            }
        }
        return result;
    }
};

struct FlowPair {
    int canvasWidth = 0, canvasHeight = 0;
    FlowField forward, backward;
    size_t Bytes() const {
        return (forward.vectors.size() + backward.vectors.size()) * sizeof(FlowVector);
    }
};

using FlowKey = std::tuple<u64, int, int, int>;
struct FlowCacheEntry {
    std::shared_ptr<const FlowPair> pair;
    uint64_t used = 0;
};
std::mutex g_flowMutex;
std::map<FlowKey, FlowCacheEntry> g_flowCache;
size_t g_flowBytes = 0;
uint64_t g_flowUse = 0;

std::vector<Pixel> MakeFlowImage(const SprImg& image, int bank, const unsigned int* palette,
    int width, int height)
{
    std::vector<Pixel> result(static_cast<size_t>(width) * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            result[static_cast<size_t>(y) * width + x] = ReadPixel(image, bank, palette,
                x - width / 2 + image.width / 2, y - height / 2 + image.height / 2);
        }
    }
    return result;
}

Pixel FlowPixel(const std::vector<Pixel>& image, int width, int height, int x, int y)
{
    return x >= 0 && y >= 0 && x < width && y < height
        ? image[static_cast<size_t>(y) * width + x] : Pixel{};
}

FlowField EstimateFlow(const std::vector<Pixel>& source, const std::vector<Pixel>& target,
    int width, int height)
{
    FlowField field;
    field.width = (width + kFlowGrid - 1) / kFlowGrid;
    field.height = (height + kFlowGrid - 1) / kFlowGrid;
    field.vectors.resize(static_cast<size_t>(field.width) * field.height);
    for (int gy = 0; gy < field.height; ++gy) {
        for (int gx = 0; gx < field.width; ++gx) {
            const int x = gx * kFlowGrid + 2, y = gy * kFlowGrid + 2;
            float signal = 0;
            for (int py = -1; py <= 1; ++py) {
                for (int px = -1; px <= 1; ++px) {
                    signal += FlowPixel(source, width, height, x + px, y + py).a;
                }
            }
            if (signal == 0) {
                continue;
            }
            int bestX = 0, bestY = 0;
            float bestError = 0;
            // Coarse-to-fine block matching, capped at 21 source pixels. ACT
            // translations are handled separately, keeping this search local.
            for (int level = 2; level >= 0; --level) {
                const int step = 1 << level;
                const int radius = level == 2 ? 4 : (level == 1 ? 2 : 1);
                const int centerX = bestX, centerY = bestY;
                float bestScore = (std::numeric_limits<float>::max)();
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int moveX = centerX + dx * step, moveY = centerY + dy * step;
                        float error = 0;
                        for (int py = -1; py <= 1; ++py) {
                            for (int px = -1; px <= 1; ++px) {
                                const Pixel a = FlowPixel(source, width, height, x + px * step, y + py * step);
                                const Pixel b = FlowPixel(target, width, height,
                                    x + px * step + moveX, y + py * step + moveY);
                                error += 2 * std::fabs(a.a - b.a) + std::fabs(a.r - b.r)
                                    + std::fabs(a.g - b.g) + std::fabs(a.b - b.b);
                            }
                        }
                        // Prefer small displacements in flat or ambiguous areas.
                        const float score = error + 0.002f * (moveX * moveX + moveY * moveY);
                        if (score < bestScore) {
                            bestScore = score;
                            bestError = error / 9;
                            bestX = moveX;
                            bestY = moveY;
                        }
                    }
                }
            }
            field.vectors[static_cast<size_t>(gy) * field.width + gx]
                = { static_cast<float>(bestX), static_cast<float>(bestY), 1 / (1 + bestError * 4) };
        }
    }
    return field;
}

void CheckConsistency(FlowField& field, const FlowField& reverse)
{
    for (int gy = 0; gy < field.height; ++gy) {
        for (int gx = 0; gx < field.width; ++gx) {
            auto& v = field.vectors[static_cast<size_t>(gy) * field.width + gx];
            const auto back = reverse.Sample(gx * kFlowGrid + 2 + v.x, gy * kFlowGrid + 2 + v.y);
            const float dx = v.x + back.x, dy = v.y + back.y;
            v.confidence *= back.confidence / (1 + (dx * dx + dy * dy) * 0.25f);
        }
    }
}

std::shared_ptr<const FlowPair> GetFlow(CSprRes& sprite, int bank, int from, int to)
{
    if (from == to) {
        return {};
    }
    const FlowKey key{ sprite.m_interpolationRevision, bank, from, to };
    {
        std::lock_guard<std::mutex> lock(g_flowMutex);
        const auto found = g_flowCache.find(key);
        if (found != g_flowCache.end()) {
            found->second.used = ++g_flowUse;
            return found->second.pair;
        }
    }
    const SprImg* a = sprite.GetSprite(bank, from);
    const SprImg* b = sprite.GetSprite(bank, to);
    if (!a || !b || a->width <= 0 || a->height <= 0 || b->width <= 0 || b->height <= 0) {
        return {};
    }
    const int width = (std::max)(a->width, b->width), height = (std::max)(a->height, b->height);
    if (width > kMaxFlowDimension || height > kMaxFlowDimension) {
        return {};
    }
    auto pair = std::make_shared<FlowPair>();
    pair->canvasWidth = width;
    pair->canvasHeight = height;
    // Geometry is estimated with the original palette and reused by dye variants.
    const auto first = MakeFlowImage(*a, bank, sprite.m_pal, width, height);
    const auto second = MakeFlowImage(*b, bank, sprite.m_pal, width, height);
    pair->forward = EstimateFlow(first, second, width, height);
    pair->backward = EstimateFlow(second, first, width, height);
    const FlowField forward = pair->forward;
    CheckConsistency(pair->forward, pair->backward);
    CheckConsistency(pair->backward, forward);
    std::lock_guard<std::mutex> lock(g_flowMutex);
    auto& entry = g_flowCache[key];
    if (entry.pair) {
        entry.used = ++g_flowUse;
        return entry.pair;
    }
    entry = { pair, ++g_flowUse };
    g_flowBytes += pair->Bytes();
    while (g_flowBytes > kFlowBudget || g_flowCache.size() > kMaxFlowPairs) {
        auto oldest = g_flowCache.end();
        for (auto it = g_flowCache.begin(); it != g_flowCache.end(); ++it) {
            if (it->first != key && (oldest == g_flowCache.end() || it->second.used < oldest->second.used)) {
                oldest = it;
            }
        }
        if (oldest == g_flowCache.end()) {
            break;
        }
        g_flowBytes -= oldest->second.pair->Bytes();
        g_flowCache.erase(oldest);
    }
    return pair;
}

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

struct ClipPose {
    const CSprClip* first = nullptr;
    const CSprClip* second = nullptr;
    const SprImg* a = nullptr;
    const SprImg* b = nullptr;
    float x = 0, y = 0, scaleX = 1, scaleY = 1, cosine = 1, sine = 0, fraction = 0;
    int width = 0, height = 0;
};

bool ResolveClipPose(CSprRes& sprite, const CSprClip& first, const CSprClip& second,
    float fraction, ClipPose* pose)
{
    pose->first = &first;
    pose->second = &second;
    // Changing banks, mirroring or layer topology cannot be matched reliably.
    if (first.clipType != second.clipType || first.flags != second.flags
        || !sprite.GetSprite(first.clipType, first.sprIndex)
        || !sprite.GetSprite(second.clipType, second.sprIndex)) {
        pose->first = pose->second = fraction < 0.5f ? &first : &second;
        fraction = 0;
    }
    pose->a = sprite.GetSprite(pose->first->clipType, pose->first->sprIndex);
    pose->b = sprite.GetSprite(pose->second->clipType, pose->second->sprIndex);
    if (!pose->a || !pose->b || pose->a->width <= 0 || pose->a->height <= 0
        || pose->b->width <= 0 || pose->b->height <= 0) {
        return false;
    }
    const auto& a = *pose->first;
    const auto& b = *pose->second;
    pose->fraction = fraction;
    pose->x = Lerp(static_cast<float>(a.x), static_cast<float>(b.x), fraction);
    pose->y = Lerp(static_cast<float>(a.y), static_cast<float>(b.y), fraction);
    if (std::fabs(pose->x) > 32768 || std::fabs(pose->y) > 32768) {
        return false;
    }
    pose->scaleX = Lerp(a.zoomX, b.zoomX, fraction);
    pose->scaleY = Lerp(a.zoomY, b.zoomY, fraction);
    if (!std::isfinite(pose->scaleX) || !std::isfinite(pose->scaleY)
        || std::fabs(pose->scaleX) < 0.001f || std::fabs(pose->scaleY) < 0.001f) {
        return false;
    }
    const float angle = (a.angle + std::remainder(static_cast<float>(b.angle) - a.angle, 360.0f) * fraction)
        * (3.14159265f / 180.0f);
    pose->cosine = std::cos(angle);
    pose->sine = std::sin(angle);
    pose->width = fraction == 0 ? pose->a->width : (std::max)(pose->a->width, pose->b->width);
    pose->height = fraction == 0 ? pose->a->height : (std::max)(pose->a->height, pose->b->height);
    const float extent = std::fabs(pose->width * pose->scaleX) + std::fabs(pose->height * pose->scaleY);
    if (!std::isfinite(extent) || extent > 4096) {
        return false;
    }
    return true;
}

RECT PoseBounds(const ClipPose& pose, float originX, float originY)
{
    float left = (std::numeric_limits<float>::max)(), top = left;
    float right = -left, bottom = right;
    for (int corner = 0; corner < 4; ++corner) {
        const float x = ((corner & 1) ? pose.width - pose.width / 2 : -pose.width / 2) * pose.scaleX;
        const float y = ((corner & 2) ? pose.height - pose.height / 2 : -pose.height / 2) * pose.scaleY;
        const float dx = originX + pose.x + x * pose.cosine - y * pose.sine;
        const float dy = originY + pose.y + x * pose.sine + y * pose.cosine;
        left = (std::min)(left, dx); right = (std::max)(right, dx);
        top = (std::min)(top, dy); bottom = (std::max)(bottom, dy);
    }
    // Include the bilinear sampling fringe, including subpixel translations.
    return { static_cast<LONG>(std::floor(left - 1)), static_cast<LONG>(std::floor(top - 1)),
        static_cast<LONG>(std::ceil(right + 1)), static_cast<LONG>(std::ceil(bottom + 1)) };
}

Pixel SamplePose(const ClipPose& pose, const FlowPair* flow, const unsigned int* palette,
    float x, float y, bool sharpPixels, float sharpness)
{
    const int bank = pose.first->clipType;
    const float t = pose.fraction;
    if (!flow || t == 0) {
        const SprImg& image = t < 0.5f ? *pose.a : *pose.b;
        return SamplePixel(image, bank, palette, x + image.width / 2, y + image.height / 2,
            sharpPixels, sharpness);
    }
    const float zx = x + flow->canvasWidth / 2, zy = y + flow->canvasHeight / 2;
    float ax = zx, ay = zy, bx = zx, by = zy;
    for (int iteration = 0; iteration < 2; ++iteration) {
        const auto f = flow->forward.Sample(ax, ay);
        const auto b = flow->backward.Sample(bx, by);
        ax = zx - t * f.x; ay = zy - t * f.y;
        bx = zx - (1 - t) * b.x; by = zy - (1 - t) * b.y;
    }
    const float confidenceA = flow->forward.Sample(ax, ay).confidence;
    const float confidenceB = flow->backward.Sample(bx, by).confidence;
    const float wa = (1 - t) * confidenceA, wb = t * confidenceB;
    if (wa + wb < 0.1f) {
        const SprImg& image = t < 0.5f ? *pose.a : *pose.b;
        return SamplePixel(image, bank, palette, x + image.width / 2, y + image.height / 2,
            sharpPixels, sharpness);
    }
    const Pixel a = SamplePixel(*pose.a, bank, palette,
        ax - flow->canvasWidth / 2 + pose.a->width / 2,
        ay - flow->canvasHeight / 2 + pose.a->height / 2, sharpPixels, sharpness);
    const Pixel b = SamplePixel(*pose.b, bank, palette,
        bx - flow->canvasWidth / 2 + pose.b->width / 2,
        by - flow->canvasHeight / 2 + pose.b->height / 2, sharpPixels, sharpness);
    if (sharpPixels) {
        // A mismatched outline must not turn gray or become a second translucent
        // outline. Keep the stronger warped sample at occlusions/color edges;
        // compatible samples still blend, and geometry continues to interpolate.
        const float colorDifference = a.a > 0.001f && b.a > 0.001f
            ? (std::max)({ std::fabs(a.r / a.a - b.r / b.a),
                std::fabs(a.g / a.a - b.g / b.a), std::fabs(a.b / a.a - b.b / b.a) })
            : 0.0f;
        if (std::fabs(a.a - b.a) > 0.25f || colorDifference > 0.15f) {
            return wa >= wb ? a : b;
        }
    }
    return a * (wa / (wa + wb)) + b * (wb / (wa + wb));
}

unsigned int PackPixel(const Pixel& pixel, const ClipPose& pose)
{
    const auto& a = *pose.first;
    const auto& b = *pose.second;
    const float t = pose.fraction;
    const float alpha = Lerp(a.a / 255.0f, b.a / 255.0f, t);
    auto byte = [](float value) {
        return static_cast<unsigned int>((std::max)(0.0f, (std::min)(255.0f, std::round(value * 255))));
    };
    return (byte(pixel.a * alpha) << 24)
        | (byte(pixel.r * alpha * Lerp(a.r / 255.0f, b.r / 255.0f, t)) << 16)
        | (byte(pixel.g * alpha * Lerp(a.g / 255.0f, b.g / 255.0f, t)) << 8)
        | byte(pixel.b * alpha * Lerp(a.b / 255.0f, b.b / 255.0f, t));
}

void Blend(unsigned int& dest, unsigned int source)
{
    const unsigned int inverse = 255 - (source >> 24);
    unsigned int result = 0;
    for (int shift = 0; shift <= 24; shift += 8) {
        const unsigned int value = ((source >> shift) & 255) + ((dest >> shift) & 255) * inverse / 255;
        result |= (std::min)(255u, value) << shift;
    }
    dest = result;
}

void ResolveMotions(const CMotion*& current, const CMotion*& next, float& fraction)
{
    fraction = std::isfinite(fraction) ? (std::max)(0.0f, (std::min)(1.0f, fraction)) : 0;
    if (!next) {
        next = current;
        fraction = 0;
    }
    if (current && next && current->sprClips.size() != next->sprClips.size()) {
        current = next = fraction < 0.5f ? current : next;
        fraction = 0;
    }
}

} // namespace

void BlitInterpolatedMotionToArgb(unsigned int* dest, int width, int height,
    float x, float y, CSprRes* sprite, const CMotion* current, const CMotion* next,
    const unsigned int* palette, float fraction, bool sharpPixels, int rasterScale, int sharpness)
{
    if (!dest || width <= 0 || height <= 0 || !sprite || !current || !palette
        || !std::isfinite(x) || !std::isfinite(y) || std::fabs(x) > 1000000 || std::fabs(y) > 1000000) {
        return;
    }
    ResolveMotions(current, next, fraction);
    rasterScale = (std::max)(1, (std::min)(4, rasterScale));
    const float inverseScale = 1.0f / rasterScale;
    const float edgeGain = static_cast<float>((std::max)(1, (std::min)(16, sharpness)));
    for (size_t index = 0; index < current->sprClips.size(); ++index) {
        ClipPose pose;
        if (!ResolveClipPose(*sprite, current->sprClips[index], next->sprClips[index], fraction, &pose)) {
            continue;
        }
        std::shared_ptr<const FlowPair> flow;
        if (pose.fraction > 0) {
            flow = GetFlow(*sprite, pose.first->clipType, pose.first->sprIndex, pose.second->sprIndex);
        }
        const RECT bounds = PoseBounds(pose, x, y);
        const int left = (std::max)(0, static_cast<int>(bounds.left) * rasterScale);
        const int top = (std::max)(0, static_cast<int>(bounds.top) * rasterScale);
        const int right = (std::min)(width, static_cast<int>(bounds.right) * rasterScale);
        const int bottom = (std::min)(height, static_cast<int>(bounds.bottom) * rasterScale);
        for (int dy = top; dy < bottom; ++dy) {
            for (int dx = left; dx < right; ++dx) {
                const float tx = (dx + 0.5f) * inverseScale - 0.5f - x - pose.x;
                const float ty = (dy + 0.5f) * inverseScale - 0.5f - y - pose.y;
                float sx = (tx * pose.cosine + ty * pose.sine) / pose.scaleX;
                const float sy = (-tx * pose.sine + ty * pose.cosine) / pose.scaleY;
                if (pose.first->flags & 1) {
                    sx = pose.width - 1 - 2 * (pose.width / 2) - sx;
                }
                Blend(dest[static_cast<size_t>(dy) * width + dx],
                    PackPixel(SamplePose(pose, flow.get(), palette, sx, sy, sharpPixels, edgeGain), pose));
            }
        }
    }
}

bool ComputeInterpolatedMotionBounds(CSprRes* sprite, const CMotion* current,
    const CMotion* next, float fraction, RECT* bounds)
{
    if (!sprite || !current || !bounds) {
        return false;
    }
    ResolveMotions(current, next, fraction);
    bool any = false;
    for (size_t index = 0; index < current->sprClips.size(); ++index) {
        ClipPose pose;
        if (!ResolveClipPose(*sprite, current->sprClips[index], next->sprClips[index], fraction, &pose)) {
            continue;
        }
        const RECT box = PoseBounds(pose, 0, 0);
        if (!any) {
            *bounds = box;
        } else {
            bounds->left = (std::min)(bounds->left, box.left);
            bounds->top = (std::min)(bounds->top, box.top);
            bounds->right = (std::max)(bounds->right, box.right);
            bounds->bottom = (std::max)(bounds->bottom, box.bottom);
        }
        any = true;
    }
    return any;
}

bool DrawInterpolatedMotionToHdc(HDC hdc, float x, float y, CSprRes* sprite,
    const CMotion* current, const CMotion* next, const unsigned int* palette, float fraction,
    bool sharpPixels, int sharpness)
{
    RECT bounds{};
    if (!hdc || !palette || !std::isfinite(x) || !std::isfinite(y)
        || std::fabs(x) > 1000000 || std::fabs(y) > 1000000
        || !ComputeInterpolatedMotionBounds(sprite, current, next, fraction, &bounds)) {
        return false;
    }
    // Account for the fractional origin when positioning the temporary surface.
    const int originX = static_cast<int>(std::floor(x)), originY = static_cast<int>(std::floor(y));
    const int width = bounds.right - bounds.left + 1, height = bounds.bottom - bounds.top + 1;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        return false;
    }
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * height);
    BlitInterpolatedMotionToArgb(pixels.data(), width, height, x - originX - bounds.left,
        y - originY - bounds.top, sprite, current, next, palette, fraction, sharpPixels, 1, sharpness);
    return AlphaBlendArgbToHdc(hdc, originX + bounds.left, originY + bounds.top,
        width, height, pixels.data(), width, height);
}

void ClearSpriteInterpolationCache()
{
    std::lock_guard<std::mutex> lock(g_flowMutex);
    g_flowCache.clear();
    g_flowBytes = 0;
}
