#pragma once

#include "UIShopCommon.h"
#include "UIWindow.h"
#include "session/Session.h"

#include <unordered_map>

struct ITEM_INFO;

class UIShortCutWnd : public UIWindow {
public:
    static constexpr int kFunctionKeySlotCount = kShortcutSlotsPerPage;
    static constexpr int kVisibleSlotCount = kShortcutVisibleSlotCount;
    static constexpr int kSlotsPerRow = kVisibleSlotCount;
    static constexpr int kRowCount = kVisibleSlotCount / kSlotsPerRow;
    static constexpr int kSlotsPerGroup = 4;
    static constexpr int kGroupsPerRow = kSlotsPerRow / kSlotsPerGroup;
    static constexpr int kIconSize = 64;
    // Reserve a one-pixel border on each side so 64x64 icons draw at native size.
    static constexpr int kSlotWidth = kIconSize + 2;
    static constexpr int kSlotLabelHeight = 16;
    static constexpr int kSlotHeight = kSlotWidth;
    static constexpr int kSlotGap = 4;
    static constexpr int kGroupGap = 16;
    static constexpr int kSlotPadding = 6;
    static constexpr int kChatGap = 4;

    static int GetWindowHeightForClientWidth(int clientWidth);

    UIShortCutWnd();
    ~UIShortCutWnd() override;

    void SetShow(int show) override;
    void OnProcess() override;
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
    int GetSlotLabelFontSize() const;
    int GetSlotLabelHeight() const;
    RECT GetSlotRect(int visibleSlot) const;
    std::string GetSlotKeyLabel(int visibleSlot) const;
    std::string GetSlotQuantityText(int visibleSlot) const;
    bool GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const;

private:
    void Layout();
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
    int m_slotSize;
    int m_maxY = 0;
    bool m_dockedToBottomLeft;
    unsigned long long m_lastDrawStateToken;
    bool m_hasDrawStateToken;
};
