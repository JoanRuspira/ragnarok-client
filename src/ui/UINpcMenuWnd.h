#pragma once

#include "UIFrameWnd.h"

#include <string>
#include <functional>
#include <vector>

class UINpcMenuWnd : public UIFrameWnd {
public:
    UINpcMenuWnd();
    ~UINpcMenuWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    void StoreInfo() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnLBtnUp(int x, int y) override;

    void SetMenu(u32 npcId,
        const std::vector<std::string>& options,
        const std::vector<u8>& optionChoices);
    void HideMenu();
    void SetChoiceMenu(const std::vector<std::string>& options, std::function<void(int)> onChoice);
    bool HandleKeyDown(int virtualKey);
    u32 GetNpcId() const;
    const std::vector<std::string>& GetOptions() const;
    int GetSelectedIndex() const;
    int GetHoverIndex() const;
    bool IsOkPressed() const;
    bool IsCancelPressed() const;
    bool GetOkRectForQt(RECT* outRect) const;
    bool GetCancelRectForQt(RECT* outRect) const;

private:
    enum class ClickTarget {
        None = 0,
        Option,
        Ok,
        Cancel,
    };

    RECT GetOptionRect(int index) const;
    RECT GetOkRect() const;
    RECT GetCancelRect() const;
    bool IsPointInRect(const RECT& rect, int x, int y) const;
    void StartDragging(int x, int y);
    void StopDragging();
    u8 GetSelectedChoice() const;
    void SubmitSelection(u8 choice);
    void DrawButton(HDC hdc, const RECT& rect, const char* label, bool hovered, bool pressed) const;

    u32 m_npcId;
    std::vector<std::string> m_options;
    std::vector<u8> m_optionChoices;
    int m_selectedIndex;
    int m_hoverIndex;
    ClickTarget m_pressedTarget;
    std::function<void(int)> m_onChoice;
};
