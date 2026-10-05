#pragma once

#include "UIWindow.h"

#include <string>

class UIExperienceBarWnd : public UIWindow {
public:
    static constexpr int kLabelFontSize = 12;
    static constexpr int kFrameWidth = 2;
    static constexpr int kHeight = kLabelFontSize + 4 + 2 * kFrameWidth;
    static constexpr int kGap = 2;
    static constexpr int kReservedHeight = kHeight + kGap;
    static constexpr COLORREF kBaseColor = RGB(48, 192, 48);
    static constexpr COLORREF kJobColor = RGB(48, 96, 220);

    UIExperienceBarWnd();
    bool ShouldDoHitTest() override { return false; }
    void OnProcess() override;
    void OnDraw() override;
    double GetBaseProgress() const { return m_baseProgress; }
    double GetJobProgress() const { return m_jobProgress; }
    std::string GetBaseProgressText() const;
    std::string GetJobProgressText() const;

private:
    double m_baseProgress = 0.0;
    double m_jobProgress = 0.0;
};
