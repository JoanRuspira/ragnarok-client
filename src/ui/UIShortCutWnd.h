#pragma once

#include "UIShopCommon.h"
#include "UIWindow.h"
#include "session/Session.h"

#include <unordered_map>

struct ITEM_INFO;

class UIShortCutWnd : public UIWindow {
public:
    static constexpr int kVisibleSlotCount = kShortcutSlotsPerPage;
    static constexpr int kSlotSize = 48;
    static constexpr int kSlotPadding = 4;

    UIShortCutWnd();
    ~UIShortCutWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void DragAndDrop(int x, int y, const DRAG_INFO* const info) override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnMouseHover(int x, int y) override;
    void OnRBtnDown(int x, int y) override;
    int GetHoverSlot() const;
    RECT GetSlotRect(int visibleSlot) const;
    std::string GetSlotDisplayText(int visibleSlot) const;
    bool GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const;

private:
    void UpdateHoverSlot(int globalX, int globalY);
    int GetSlotAtGlobalPoint(int globalX, int globalY) const;
    void ActivateSlot(int visibleSlot);
    const shopui::BitmapPixels* GetItemIcon(const ITEM_INFO& item);
    const shopui::BitmapPixels* GetSkillIcon(int skillId);
    unsigned long long BuildDisplayStateToken() const;

    std::unordered_map<unsigned int, shopui::BitmapPixels> m_itemIconCache;
    std::unordered_map<int, shopui::BitmapPixels> m_skillIconCache;
    int m_hoverSlot;
    int m_pressedSlot;
    int m_pressedSlotAbsoluteIndex;
    int m_slotPressArmed;
    int m_isDragging;
    int m_startGlobalX;
    int m_startGlobalY;
    int m_orgX;
    int m_orgY;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};
