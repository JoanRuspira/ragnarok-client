#pragma once

#include "UIWindow.h"

class UIExperienceBarWnd : public UIWindow {
public:
    static constexpr int kHeight = 6;
    static constexpr int kGap = 2;
    static constexpr int kReservedHeight = kHeight + kGap;
    static constexpr COLORREF kBaseColor = RGB(82, 197, 110);
    static constexpr COLORREF kJobColor = RGB(67, 151, 241);

    UIExperienceBarWnd();
    bool ShouldDoHitTest() override { return false; }
    void OnProcess() override;
    void OnDraw() override;
    double GetBaseProgress() const { return m_baseProgress; }
    double GetJobProgress() const { return m_jobProgress; }

private:
    double m_baseProgress = 0.0;
    double m_jobProgress = 0.0;
};
