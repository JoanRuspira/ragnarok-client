#pragma once

#include "RenderBackend.h"

#include <vector>

enum class WindowMode {
    Windowed = 0,
    Fullscreen,
    BorderlessFullscreen,
};

enum class AntiAliasingMode {
    None = 0,
    FXAA,
    SMAA,
};

enum class SmaaPreset {
    High = 0,
};

struct GraphicsSettings {
    int width;
    int height;
    WindowMode windowMode;
    int textureUpscaleFactor;
    int anisotropicLevel;
    AntiAliasingMode antiAliasing;
    int frameRate; // 0 follows the current monitor; otherwise an explicit FPS cap.
    bool verticalSync;
    bool spriteInterpolation;
    bool sharpSprites;
    int spriteSharpness; // Pixel-edge gain: 1..16, used when sharpSprites is enabled.
};

GraphicsSettings LoadGraphicsSettings();
bool SaveGraphicsSettings(const GraphicsSettings& settings);
GraphicsSettings GetDefaultGraphicsSettings();
const GraphicsSettings& GetCachedGraphicsSettings();
void RefreshGraphicsSettingsCache();
void SanitizeGraphicsSettings(GraphicsSettings* settings);

bool DoesBackendSupportWindowMode(RenderBackendType backend, WindowMode mode);
bool DoesBackendSupportResolutionSelection(RenderBackendType backend);
bool DoesBackendSupportTextureUpscaling(RenderBackendType backend);
bool DoesBackendSupportAnisotropicFiltering(RenderBackendType backend);
bool DoesBackendSupportAntiAliasing(RenderBackendType backend);
std::vector<AntiAliasingMode> GetSupportedAntiAliasingModesForBackend(RenderBackendType backend);
bool DoesBackendSupportAntiAliasingMode(RenderBackendType backend, AntiAliasingMode mode);
AntiAliasingMode GetEffectiveAntiAliasingModeForBackend(RenderBackendType backend, AntiAliasingMode requestedMode);
SmaaPreset GetDefaultSmaaPreset();
const char* GetSmaaPresetName(SmaaPreset preset);
void ClampGraphicsSettingsToBackend(RenderBackendType backend, GraphicsSettings* settings);
WindowMode GetEffectiveWindowModeForBackend(RenderBackendType backend, WindowMode requestedMode);
bool GraphicsSettingsRequireRestart(const GraphicsSettings& current, const GraphicsSettings& pending);
