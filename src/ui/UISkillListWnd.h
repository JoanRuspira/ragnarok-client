#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "skill/Skill.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct PLAYER_SKILL_INFO;

class UISkillListWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string label;
        bool visible = true;
    };

    struct DisplayLevelSelector {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int direction = 0;
        bool enabled = false;
        bool pressed = false;
    };

    struct DisplayRow {
        int skillId = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int iconX = 0;
        int iconY = 0;
        int iconFrameWidth = 1;
        int levelX = 0;
        int levelY = 0;
        int levelSize = 22;
        bool levelSelectionVisible = false;
        std::array<DisplayLevelSelector, 2> levelSelectors{};
        bool iconVisible = true;
        bool learned = false;
        bool selected = false;
        bool hovered = false;
        bool upgradeVisible = false;
        bool upgradePressed = false;
        int upgradeX = 0;
        int upgradeY = 0;
        int upgradeWidth = 0;
        int upgradeHeight = 0;
        std::string levelText;
    };

    struct DisplayButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        bool hovered = false;
        bool pressed = false;
        bool selected = false;
        std::string label;
    };

    struct DisplayColumn {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::string title;
    };

    struct DisplayConnection {
        int x = 0;
        int y = 0;
        int width = 22;
        int height = 0;
        int shaftWidth = 8;
        int headHeight = 10;
        bool active = false;
    };

    struct DisplayScrollBar {
        bool visible = false;
        bool dragging = false;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int thumbY = 0;
        int thumbHeight = 0;
    };

    struct DisplayData {
        int skillPointCount = 0;
        int activeTab = 0;
        int iconSize = 64;
        int viewportX = 0;
        int viewportY = 0;
        int viewportWidth = 0;
        int viewportHeight = 0;
        DisplayScrollBar scrollBar;
        std::vector<DisplayButton> tabs;
        std::vector<DisplayColumn> columns;
        std::vector<DisplayConnection> connections;
        std::vector<DisplayRow> rows;
        std::vector<DisplayButton> bottomButtons;
    };

    UISkillListWnd();
    ~UISkillListWnd() override;

    void SetShow(int show) override;
    void Move(int x, int y) override;
    bool IsUpdateNeed() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnCreate(int x, int y) override;
    void OnDestroy() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnRBtnDown(int x, int y) override;
    void OnMouseMove(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    bool GetDisplayDataForQt(DisplayData* outData);
    int GetQtSystemButtonCount() const;
    bool GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const;

private:
    struct TextButton {
        RECT rect{};
        std::string label;
        bool hovered = false;
        bool pressed = false;
    };

    struct VisibleSkill {
        int skillId = 0;
        const PLAYER_SKILL_INFO* skill = nullptr;
        RECT rowRect{};
        RECT upgradeRect{};
        std::array<RECT, 2> levelRects{};
    };

    void EnsureCreated();
    void LayoutChildren();
    void RefreshVisibleSkillsForInteractionState();
    void UpdateHover(int globalX, int globalY);
    void SelectTab(int index);
    bool CanSelectSkillLevel(int skillId) const;
    int GetSelectedSkillLevel(int skillId) const;
    void AdjustSelectedSkillLevel(int skillId, int direction);
    RECT GetSkillViewportRect() const;
    RECT GetScrollTrackRect() const;
    RECT GetScrollThumbRect() const;
    int GetMaxScrollOffset() const;
    void SetScrollOffset(int offset);
    void UpdateScrollFromThumbPosition(int globalY);
    void DrawWindowChrome(HDC hdc) const;
    void DrawButton(HDC hdc, const DisplayButton& button) const;
    int HitTestBottomButton(int globalX, int globalY) const;
    const shopui::BitmapPixels* GetSkillIcon(int skillId, bool grayscale);
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated = false;
    int m_hoveredRow = -1;
    int m_activeTab = 0;
    int m_scrollOffset = 0;
    bool m_draggingScrollThumb = false;
    int m_scrollDragOffsetY = 0;
    int m_layoutJobId = -1;
    int m_pressedTab = -1;
    int m_pressedSystemButton = -1;
    int m_selectedSkillId = 0;
    int m_pressedUpgradeSkillId = 0;
    int m_pressedLevelSkillId = 0;
    int m_pressedLevelDirection = 0;
    unsigned int m_levelSelectionCharacterId = 0;
    std::unordered_map<int, int> m_selectedSkillLevels;
    int m_dragSkillId = 0;
    int m_dragSkillLevel = 0;
    bool m_dragArmed = false;
    POINT m_dragStartPoint{};
    std::array<SkillTreeLayout, kSkillTreeJobTabs> m_jobTrees{};
    std::array<TextButton, kSkillTreeJobTabs> m_tabs{};
    std::array<TextButton, 2> m_bottomButtons{};
    std::unordered_map<unsigned long long, shopui::BitmapPixels> m_iconCache;
    std::vector<VisibleSkill> m_visibleSkills;
    unsigned long long m_lastVisualStateToken = 0;
    bool m_hasVisualStateToken = false;
};
