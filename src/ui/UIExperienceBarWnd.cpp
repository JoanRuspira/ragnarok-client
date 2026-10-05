#include "UIExperienceBarWnd.h"

#include "UiScale.h"
#include "UiTheme.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace {

std::string FormatExperienceProgress(double progress)
{
    const int tenths = static_cast<int>(std::lround(std::clamp(progress, 0.0, 1.0) * 1000.0));
    if (tenths == 0 || tenths == 1000) {
        return std::to_string(tenths / 10) + "%";
    }
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}

HFONT GetExperienceLabelFont()
{
#if RO_PLATFORM_WINDOWS
    static HFONT font = CreateFontA(-UIExperienceBarWnd::kLabelFontSize, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    return font ? font : shopui::GetUiFont();
#else
    return shopui::GetUiFont();
#endif
}

void DrawExperienceTextPass(HDC hdc, RECT rect, const std::string& text, COLORREF color)
{
    const UINT flags = DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
#if RO_ENABLE_QT6_UI
    shopui::DrawWindowTextRectQt(hdc, rect, text, color, flags);
#else
    SetTextColor(hdc, color);
    DrawTextA(hdc, text.c_str(), -1, &rect, flags);
#endif
}

void DrawExperienceLabel(HDC hdc, const RECT& rect, const std::string& text)
{
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldTextColor = SetTextColor(hdc, RGB(0, 0, 0));
    HGDIOBJ oldFont = SelectObject(hdc, GetExperienceLabelFont());

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            RECT outlineRect = rect;
            OffsetRect(&outlineRect, dx, dy);
            DrawExperienceTextPass(hdc, outlineRect, text, RGB(0, 0, 0));
        }
    }
    DrawExperienceTextPass(hdc, rect, text, uitheme::kUiText);
    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
    SelectObject(hdc, oldFont);
}

} // namespace

UIExperienceBarWnd::UIExperienceBarWnd()
{
    Create(1, kHeight);
    OnProcess();
}

void UIExperienceBarWnd::OnProcess()
{
    RECT clientRect{ 0, 0, WINDOW_WIDTH > 0 ? WINDOW_WIDTH : 640, WINDOW_HEIGHT > 0 ? WINDOW_HEIGHT : 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }
    const bool scaledUi = IsQtUiRuntimeEnabled();
    const int width = (std::max)(1, scaledUi
        ? UiScaleRawToLogicalCoordinate(clientRect.right - clientRect.left)
        : static_cast<int>(clientRect.right - clientRect.left));
    const int height = (std::max)(1, scaledUi
        ? UiScaleRawToLogicalCoordinate(clientRect.bottom - clientRect.top)
        : static_cast<int>(clientRect.bottom - clientRect.top));
    Resize(width, (std::min)(kHeight, height));
    Move(0, height - m_h);

    const double baseProgress = g_session.GetBaseExpProgress();
    const double jobProgress = g_session.GetJobExpProgress();
    if (baseProgress != m_baseProgress || jobProgress != m_jobProgress) {
        m_baseProgress = baseProgress;
        m_jobProgress = jobProgress;
        Invalidate();
    }
}

std::string UIExperienceBarWnd::GetBaseProgressText() const
{
    return FormatExperienceProgress(m_baseProgress);
}

std::string UIExperienceBarWnd::GetJobProgressText() const
{
    return FormatExperienceProgress(m_jobProgress);
}

void UIExperienceBarWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_isDirty = 0;
        return;
    }
    if (m_show == 0) {
        return;
    }
    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const int midpoint = m_x + m_w / 2;
    const RECT bars[2] = {
        { m_x, m_y, midpoint, m_y + m_h },
        { midpoint, m_y, m_x + m_w, m_y + m_h }
    };
    const double progress[2] = { m_baseProgress, m_jobProgress };
    const COLORREF colors[2] = { kBaseColor, kJobColor };
    const std::string labels[2] = { GetBaseProgressText(), GetJobProgressText() };
    for (int index = 0; index < 2; ++index) {
        const RECT& bar = bars[index];
        shopui::FillRectColor(hdc, bar, uitheme::kPanelBackground);
        const int innerWidth = (std::max)(0, static_cast<int>(bar.right - bar.left) - 2 * kFrameWidth);
        const int fillWidth = static_cast<int>(std::lround(innerWidth * progress[index]));
        if (fillWidth > 0 && m_h > 2 * kFrameWidth) {
            const RECT fill{ bar.left + kFrameWidth, bar.top + kFrameWidth,
                bar.left + kFrameWidth + fillWidth, bar.bottom - kFrameWidth };
            shopui::FillRectColor(hdc, fill, colors[index]);
        }
        for (int inset = 0; inset < kFrameWidth; ++inset) {
            const RECT frame{ bar.left + inset, bar.top + inset, bar.right - inset, bar.bottom - inset };
            uitheme::FrameRoundedRect(hdc, frame, uitheme::kFrame, (std::max)(0, uitheme::kCornerRadius - inset));
        }
        const RECT labelRect{ bar.left + kFrameWidth, bar.top + kFrameWidth,
            bar.right - kFrameWidth, bar.bottom - kFrameWidth };
        DrawExperienceLabel(hdc, labelRect, labels[index]);
    }
    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}
