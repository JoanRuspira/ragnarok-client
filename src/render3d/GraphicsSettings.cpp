#include "GraphicsSettings.h"

#include "core/SettingsIni.h"

#include <windows.h>

#include <algorithm>
#include <vector>

namespace {

constexpr char kGraphicsSection[] = "Graphics";
constexpr char kGraphicsWidthValue[] = "Width";
constexpr char kGraphicsHeightValue[] = "Height";
constexpr char kGraphicsWindowModeValue[] = "WindowMode";
constexpr char kGraphicsTextureUpscaleValue[] = "TextureUpscale";
constexpr char kGraphicsAnisotropicValue[] = "AnisotropicLevel";
constexpr char kGraphicsAntiAliasingValue[] = "AntiAliasing";
constexpr char kGraphicsFrameRateValue[] = "FrameRate";
constexpr char kGraphicsVerticalSyncValue[] = "VerticalSync";
constexpr int kMinWidth = 640;
constexpr int kMinHeight = 480;
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;
GraphicsSettings g_cachedSettings = GetDefaultGraphicsSettings();
bool g_cachedSettingsValid = false;

int ClampToAllowedAnisotropy(int level)
{
    static constexpr int kAllowedLevels[] = { 1, 2, 4, 8, 16 };
    int best = kAllowedLevels[0];
    for (int candidate : kAllowedLevels) {
        if (level >= candidate) {
            best = candidate;
        }
    }
    return best;
}

} // namespace

GraphicsSettings GetDefaultGraphicsSettings()
{
    GraphicsSettings settings{};
    settings.width = kMinWidth;
    settings.height = kMinHeight;
    settings.windowMode = WindowMode::Windowed;
    settings.textureUpscaleFactor = 1;
    settings.anisotropicLevel = 1;
    settings.antiAliasing = AntiAliasingMode::None;
    settings.frameRate = 0;
    settings.verticalSync = true;
    return settings;
}

void SanitizeGraphicsSettings(GraphicsSettings* settings)
{
    if (!settings) {
        return;
    }

    settings->width = (std::max)(kMinWidth, (std::min)(kMaxWidth, settings->width));
    settings->height = (std::max)(kMinHeight, (std::min)(kMaxHeight, settings->height));

    if (settings->windowMode != WindowMode::Windowed
        && settings->windowMode != WindowMode::Fullscreen
        && settings->windowMode != WindowMode::BorderlessFullscreen) {
        settings->windowMode = WindowMode::Windowed;
    }

    settings->textureUpscaleFactor = (std::max)(1, (std::min)(4, settings->textureUpscaleFactor));
    settings->anisotropicLevel = ClampToAllowedAnisotropy(settings->anisotropicLevel);
    settings->frameRate = settings->frameRate <= 0
        ? 0
        : (std::max)(30, (std::min)(360, settings->frameRate));
    if (settings->antiAliasing != AntiAliasingMode::None
        && settings->antiAliasing != AntiAliasingMode::FXAA
        && settings->antiAliasing != AntiAliasingMode::SMAA) {
        settings->antiAliasing = AntiAliasingMode::None;
    }
}

GraphicsSettings LoadGraphicsSettings()
{
    GraphicsSettings settings = GetDefaultGraphicsSettings();

    settings.width = LoadSettingsIniInt(kGraphicsSection, kGraphicsWidthValue, settings.width);
    settings.height = LoadSettingsIniInt(kGraphicsSection, kGraphicsHeightValue, settings.height);
    settings.windowMode = static_cast<WindowMode>(LoadSettingsIniInt(kGraphicsSection, kGraphicsWindowModeValue, static_cast<int>(settings.windowMode)));
    settings.textureUpscaleFactor = LoadSettingsIniInt(kGraphicsSection, kGraphicsTextureUpscaleValue, settings.textureUpscaleFactor);
    settings.anisotropicLevel = LoadSettingsIniInt(kGraphicsSection, kGraphicsAnisotropicValue, settings.anisotropicLevel);
    settings.antiAliasing = static_cast<AntiAliasingMode>(LoadSettingsIniInt(kGraphicsSection, kGraphicsAntiAliasingValue, static_cast<int>(settings.antiAliasing)));
    settings.frameRate = LoadSettingsIniInt(kGraphicsSection, kGraphicsFrameRateValue, settings.frameRate);
    settings.verticalSync = LoadSettingsIniInt(kGraphicsSection, kGraphicsVerticalSyncValue, settings.verticalSync ? 1 : 0) != 0;

    SanitizeGraphicsSettings(&settings);
    return settings;
}

bool SaveGraphicsSettings(const GraphicsSettings& rawSettings)
{
    GraphicsSettings settings = rawSettings;
    SanitizeGraphicsSettings(&settings);

    return SaveSettingsIniInt(kGraphicsSection, kGraphicsWidthValue, settings.width)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsHeightValue, settings.height)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsWindowModeValue, static_cast<int>(settings.windowMode))
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsTextureUpscaleValue, settings.textureUpscaleFactor)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsAnisotropicValue, settings.anisotropicLevel)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsAntiAliasingValue, static_cast<int>(settings.antiAliasing))
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsFrameRateValue, settings.frameRate)
        && SaveSettingsIniInt(kGraphicsSection, kGraphicsVerticalSyncValue, settings.verticalSync ? 1 : 0);
}

const GraphicsSettings& GetCachedGraphicsSettings()
{
    if (!g_cachedSettingsValid) {
        g_cachedSettings = LoadGraphicsSettings();
        g_cachedSettingsValid = true;
    }
    return g_cachedSettings;
}

void RefreshGraphicsSettingsCache()
{
    g_cachedSettings = LoadGraphicsSettings();
    g_cachedSettingsValid = true;
}

bool DoesBackendSupportWindowMode(RenderBackendType backend, WindowMode mode)
{
    switch (backend) {
    case RenderBackendType::LegacyDirect3D7:
        return mode == WindowMode::Windowed
            || mode == WindowMode::Fullscreen
            || mode == WindowMode::BorderlessFullscreen;

    case RenderBackendType::Direct3D11:
    case RenderBackendType::Direct3D12:
    case RenderBackendType::Vulkan:
        return mode == WindowMode::Windowed || mode == WindowMode::BorderlessFullscreen;

    default:
        return mode == WindowMode::Windowed;
    }
}

bool DoesBackendSupportResolutionSelection(RenderBackendType backend)
{
    return backend == RenderBackendType::LegacyDirect3D7
        || backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

bool DoesBackendSupportTextureUpscaling(RenderBackendType backend)
{
    return backend == RenderBackendType::LegacyDirect3D7
        || backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

bool DoesBackendSupportAnisotropicFiltering(RenderBackendType backend)
{
    return backend == RenderBackendType::Direct3D11
        || backend == RenderBackendType::Direct3D12
        || backend == RenderBackendType::Vulkan;
}

std::vector<AntiAliasingMode> GetSupportedAntiAliasingModesForBackend(RenderBackendType backend)
{
    switch (backend) {
    case RenderBackendType::Direct3D11:
    case RenderBackendType::Direct3D12:
    case RenderBackendType::Vulkan:
        return { AntiAliasingMode::None, AntiAliasingMode::FXAA, AntiAliasingMode::SMAA };

    default:
        return {};
    }
}

bool DoesBackendSupportAntiAliasing(RenderBackendType backend)
{
    return !GetSupportedAntiAliasingModesForBackend(backend).empty();
}

bool DoesBackendSupportAntiAliasingMode(RenderBackendType backend, AntiAliasingMode mode)
{
    if (mode == AntiAliasingMode::None) {
        return true;
    }

    const std::vector<AntiAliasingMode> supportedModes = GetSupportedAntiAliasingModesForBackend(backend);
    return std::find(supportedModes.begin(), supportedModes.end(), mode) != supportedModes.end();
}

AntiAliasingMode GetEffectiveAntiAliasingModeForBackend(RenderBackendType backend, AntiAliasingMode requestedMode)
{
    return DoesBackendSupportAntiAliasingMode(backend, requestedMode)
        ? requestedMode
        : AntiAliasingMode::None;
}

SmaaPreset GetDefaultSmaaPreset()
{
    return SmaaPreset::High;
}

const char* GetSmaaPresetName(SmaaPreset preset)
{
    switch (preset) {
    case SmaaPreset::High:
        return "High";

    default:
        return "High";
    }
}

void ClampGraphicsSettingsToBackend(RenderBackendType backend, GraphicsSettings* settings)
{
    if (!settings) {
        return;
    }

    settings->windowMode = GetEffectiveWindowModeForBackend(backend, settings->windowMode);
    if (!DoesBackendSupportAnisotropicFiltering(backend)) {
        settings->anisotropicLevel = 1;
    }
    settings->antiAliasing = GetEffectiveAntiAliasingModeForBackend(backend, settings->antiAliasing);
}

WindowMode GetEffectiveWindowModeForBackend(RenderBackendType backend, WindowMode requestedMode)
{
    if (DoesBackendSupportWindowMode(backend, requestedMode)) {
        return requestedMode;
    }

    if (DoesBackendSupportWindowMode(backend, WindowMode::BorderlessFullscreen)) {
        return WindowMode::BorderlessFullscreen;
    }

    return WindowMode::Windowed;
}

bool GraphicsSettingsRequireRestart(const GraphicsSettings& current, const GraphicsSettings& pending)
{
    return current.width != pending.width
        || current.height != pending.height
        || current.windowMode != pending.windowMode
        || current.textureUpscaleFactor != pending.textureUpscaleFactor
        || current.anisotropicLevel != pending.anisotropicLevel
        || current.antiAliasing != pending.antiAliasing;
}
