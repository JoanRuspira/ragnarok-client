#include "UIExperienceBarWnd.h"

#include "UiScale.h"
#include "UiTheme.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "session/Session.h"

#include <algorithm>
#include <cmath>

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
    for (int index = 0; index < 2; ++index) {
        const RECT& bar = bars[index];
        shopui::FillRectColor(hdc, bar, uitheme::kPanelBackground);
        const int innerWidth = (std::max)(0, static_cast<int>(bar.right - bar.left) - 2);
        const int fillWidth = static_cast<int>(std::lround(innerWidth * progress[index]));
        if (fillWidth > 0 && m_h > 2) {
            const RECT fill{ bar.left + 1, bar.top + 1, bar.left + 1 + fillWidth, bar.bottom - 1 };
            shopui::FillRectColor(hdc, fill, colors[index]);
        }
        shopui::FrameRectColor(hdc, bar, uitheme::kFrame);
    }
    ReleaseDrawTarget(hdc);
    m_isDirty = 0;
}
