#pragma once

#include "platform/WindowsCompat.h"

// Shared UI colors and typography. Native drawing and QML (through
// QtUiState.uiTheme) both read these constants.
namespace uitheme {

inline constexpr char kUiFontFamily[] = "Trajan Pro";

void InitializeFonts();

inline constexpr COLORREF kPanelBackground = RGB(32, 46, 92);
inline constexpr COLORREF kControlBackground = RGB(32, 32, 32);
inline constexpr COLORREF kControlBackgroundText = RGB(255, 255, 255); // Textbox background.
inline constexpr COLORREF kFrame = RGB(242, 206, 150);
inline constexpr COLORREF kAccent = RGB(230, 198, 88);
inline constexpr COLORREF kUiText = RGB(242, 206, 150);
inline constexpr COLORREF kWritableText = RGB(32, 32, 32);
inline constexpr COLORREF kShadow = RGB(8, 12, 16);
inline constexpr COLORREF kControlHover = RGB(65, 75, 88);
inline constexpr COLORREF kAccentHover = RGB(245, 214, 108);
inline constexpr int kPanelAlpha = 230;

} // namespace uitheme
