#include "UIRechargeGage.h"

#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"

#include <algorithm>
#include <windows.h>

UIRechargeGage::UIRechargeGage() = default;

void UIRechargeGage::SetAmount(int amount, int totalAmount)
{
    m_amount = amount;
    m_totalAmount = totalAmount;
    Invalidate();
}

int UIRechargeGage::GetAmount() const
{
    return m_amount;
}

int UIRechargeGage::GetTotalAmount() const
{
    return m_totalAmount;
}

void UIRechargeGage::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    uitheme::FillRoundedRect(hdc, outer, RGB(0, 0, 0), 0);

    constexpr int border = uitheme::kActorVitalsBorderThickness;
    RECT inner{ m_x + border, m_y + border, m_x + m_w - border, m_y + m_h - border };
    uitheme::FillRoundedRect(hdc, inner, RGB(255, 255, 255), 0);

    const int total = (std::max)(1, m_totalAmount);
    const int amount = (std::max)(0, (std::min)(m_amount, total));
    if (amount > 0) {
        const int fillW = static_cast<int>(static_cast<double>(inner.right - inner.left) * amount / total);
        if (fillW > 0) {
            RECT fill{ inner.left, inner.top, inner.left + fillW, inner.bottom };
            uitheme::FillRoundedRect(hdc, fill, uitheme::kCastBar, 0);
        }
    }

    DrawChildrenToHdc(hdc);
    ReleaseDrawTarget(hdc);
}
