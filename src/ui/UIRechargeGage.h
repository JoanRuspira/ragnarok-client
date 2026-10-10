#pragma once

#include "UIWindow.h"

// Ref UIRechargeGage — small horizontal cast / recharge progress bar.
class UIRechargeGage : public UIWindow {
public:
    UIRechargeGage();

    void SetAmount(int amount, int totalAmount);
    void OnDraw() override;
    int GetAmount() const;
    int GetTotalAmount() const;

private:
    int m_amount = 0;
    int m_totalAmount = 0;
};
