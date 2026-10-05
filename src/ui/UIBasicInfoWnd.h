#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"

#include <string>

class QImage;

class UIBasicInfoWnd : public UIFrameWnd {
public:
    static constexpr int kPortraitDiameter = 67;
    static constexpr int kPortraitY = 0;
    static constexpr int kPanelOffsetX = kPortraitDiameter - 6;

    struct DisplayData {
        std::string name;
        int level = 0;
        int hp = 0;
        int maxHp = 0;
        int sp = 0;
        int maxSp = 0;
        std::string weightText;
        std::string zenyText;
    };

    UIBasicInfoWnd();

    bool IsUpdateNeed() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void StoreInfo() override;

    bool GetDisplayDataForQt(DisplayData* outData) const;
    unsigned long long GetPortraitRevision() const;
    bool BuildQtPortraitImage(QImage* outImage) const;

private:
    DisplayData BuildDisplayData() const;
    unsigned long long BuildDisplayStateToken() const;
    const shopui::BitmapPixels& GetPortraitPixels() const;

    unsigned long long m_lastDrawStateToken = 0;
    bool m_hasDrawStateToken = false;
    mutable shopui::BitmapPixels m_portraitPixels;
    mutable unsigned long long m_lastPortraitRevision = 0;
    mutable bool m_hasPortraitPixels = false;
};
