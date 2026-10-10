#pragma once

#include "platform/WindowsCompat.h"

// Shared UI colors, typography and geometry. Native drawing and QML (through
// QtUiState.uiTheme) both read these constants.
namespace uitheme {

inline constexpr char kUiFontFamily[] = "Trajan Pro";
// Corner radius in logical pixels. Set to zero for square corners.
inline constexpr int kCornerRadius = 6;

void InitializeFonts();
void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF color, int radius = kCornerRadius);
void FrameRoundedRect(HDC hdc, const RECT& rect, COLORREF color, int radius = kCornerRadius);

inline constexpr COLORREF kPanelBackground = RGB(21, 42, 76);
inline constexpr COLORREF kControlBackground = RGB(32, 32, 32);
inline constexpr COLORREF kControlBackgroundText = RGB(255, 255, 255); // Textbox background.
inline constexpr COLORREF kFrame = RGB(242, 206, 150);
inline constexpr COLORREF kAccent = RGB(230, 198, 88);
inline constexpr COLORREF kUiText = RGB(242, 206, 150);
inline constexpr COLORREF kWritableText = RGB(32, 32, 32);
inline constexpr COLORREF kShadow = RGB(8, 12, 16);
inline constexpr COLORREF kControlHover = RGB(65, 75, 88);
inline constexpr COLORREF kAccentHover = RGB(245, 214, 108);
inline constexpr COLORREF kHpBar = RGB(48, 192, 48);
inline constexpr COLORREF kSpBar = RGB(48, 96, 220);
inline constexpr COLORREF kCastBar = RGB(252, 186, 3);
// Inner dimensions shared by the actor health and casting bars, in screen pixels.
inline constexpr int kActorVitalsBarWidth = 56;
inline constexpr int kActorVitalsBarHeight = 4;
inline constexpr int kActorVitalsBorderThickness = 1;
inline constexpr int kPanelAlpha = 230;

} // namespace uitheme
