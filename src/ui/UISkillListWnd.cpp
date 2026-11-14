#include "UISkillListWnd.h"
#include "ui/UiTheme.h"

#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "UIShortCutWnd.h"
#include "UIWindowMgr.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "session/Session.h"

#include <windows.h>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>

namespace {

constexpr int kColumnWidth = 140;
constexpr int kColumnGap = 12;
constexpr int kGridLeft = 16;
constexpr int kGridTop = 84;
constexpr int kRowHeight = 112;
constexpr int kCellHeight = 86;
constexpr int kIconSize = 64;
constexpr int kKnownIconFrameWidth = 4;
constexpr int kUnknownIconFrameWidth = 1;
constexpr int kLevelBadgeSize = 22;
constexpr int kLevelBadgeOverlap = 10;
constexpr int kLevelSelectorWidth = 14;
constexpr int kLevelSelectorHeight = 18;
constexpr int kLevelSelectorGap = 2;
constexpr int kWindowWidth = kGridLeft * 2 + kSkillTreeColumns * kColumnWidth + (kSkillTreeColumns - 1) * kColumnGap;
constexpr int kWindowHeight = 680;
constexpr int kArrowWidth = 22;
constexpr int kArrowShaftWidth = 8;
constexpr int kArrowHeadHeight = 10;
constexpr int kArrowGap = 4;
constexpr int kTitleBarHeight = 17;
constexpr int kQtButtonWidth = 12;
constexpr int kQtButtonHeight = 11;
constexpr int kButtonIdBase = 148;
constexpr int kButtonIdMini = 149;
constexpr int kButtonIdClose = 150;
constexpr int kBottomButtonUse = 0;
constexpr int kBottomButtonClose = 1;

void HashTokenValue(unsigned long long* hash, unsigned long long value)
{
    if (!hash) {
        return;
    }
    *hash ^= value;
    *hash *= 1099511628211ull;
}

void HashTokenString(unsigned long long* hash, const std::string& value)
{
    if (!hash) {
        return;
    }
    for (unsigned char ch : value) {
        HashTokenValue(hash, static_cast<unsigned long long>(ch));
    }
    HashTokenValue(hash, 0xFFull);
}

#if RO_ENABLE_QT6_UI
QFont BuildSkillListFontFromHdc(HDC hdc)
{
    LOGFONTA logFont{};
    if (hdc) {
        if (HGDIOBJ fontObject = GetCurrentObject(hdc, OBJ_FONT)) {
            GetObjectA(fontObject, sizeof(logFont), &logFont);
        }
    }

    const QString family = logFont.lfFaceName[0] != '\0'
        ? QString::fromLocal8Bit(logFont.lfFaceName)
        : QString::fromLatin1(uitheme::kUiFontFamily);
    QFont font(family);
    font.setPixelSize(logFont.lfHeight != 0 ? (std::max)(1, static_cast<int>(std::abs(logFont.lfHeight))) : 13);
    font.setBold(logFont.lfWeight >= FW_BOLD);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawSkillTextLineQt(HDC hdc, int x, int y, COLORREF color, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str());
    if (label.isEmpty()) {
        return;
    }

    const QFont font = BuildSkillListFontFromHdc(hdc);
    const QFontMetrics metrics(font);
    const int width = (std::max)(1, metrics.horizontalAdvance(label) + 4);
    const int height = (std::max)(1, metrics.height() + 2);
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(0, metrics.ascent(), label);
    AlphaBlendArgbToHdc(hdc, x, y, width, height, pixels.data(), width, height);
}

void DrawSkillButtonTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color)
{
    if (!hdc || text.empty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    std::vector<unsigned int> pixels(static_cast<size_t>(width) * static_cast<size_t>(height), 0u);
    QImage image(reinterpret_cast<uchar*>(pixels.data()), width, height, width * static_cast<int>(sizeof(unsigned int)), QImage::Format_ARGB32);
    if (image.isNull()) {
        return;
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(BuildSkillListFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignCenter | Qt::AlignVCenter | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawTextLine(HDC hdc, int x, int y, COLORREF color, const std::string& text)
{
    if (!hdc || text.empty()) {
        return;
    }
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
#if RO_ENABLE_QT6_UI
    DrawSkillTextLineQt(hdc, x, y, color, text);
#else
    TextOutA(hdc, x, y, text.c_str(), static_cast<int>(text.size()));
#endif
}

void FillRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    uitheme::FillRoundedRect(hdc, rect, color);
}

void FrameRectColor(HDC hdc, const RECT& rect, COLORREF color)
{
    uitheme::FrameRoundedRect(hdc, rect, color);
}

bool IsInsideRect(const RECT& rect, int x, int y)
{
    return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
}

RECT MakeSkillRect(int x, int y, int left, int top, int width, int height)
{
    RECT rect{ x + left, y + top, x + left + width, y + top + height };
    return rect;
}

HFONT GetUiFont()
{
    static HFONT s_font = CreateFontA(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    return s_font;
}

HFONT GetUiBoldFont()
{
    static HFONT s_font = CreateFontA(
        -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    return s_font;
}


RECT GetIconRect(const RECT& cell)
{
    const int x = cell.left + (kColumnWidth - kIconSize) / 2;
    return RECT{ x, cell.top + 2, x + kIconSize, cell.top + 2 + kIconSize };
}

RECT GetLevelSelectorRect(const RECT& cell, int direction)
{
    const RECT icon = GetIconRect(cell);
    const int badgeX = icon.right - kLevelBadgeOverlap;
    const int x = direction < 0 ? badgeX - kLevelSelectorGap - kLevelSelectorWidth
        : badgeX + kLevelBadgeSize + kLevelSelectorGap;
    const int y = icon.bottom - kLevelBadgeOverlap + (kLevelBadgeSize - kLevelSelectorHeight) / 2;
    return RECT{ x, y, x + kLevelSelectorWidth, y + kLevelSelectorHeight };
}

void DrawCenteredText(HDC hdc, const RECT& rect, const std::string& text, COLORREF color)
{
#if RO_ENABLE_QT6_UI
    DrawSkillButtonTextQt(hdc, rect, text, color);
#else
    RECT textRect = rect;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextA(hdc, text.c_str(), static_cast<int>(text.size()), &textRect,
        DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
#endif
}

void DrawSkillArrow(HDC hdc, const UISkillListWnd::DisplayConnection& arrow)
{
    const int center = arrow.x + arrow.width / 2;
    const int top = arrow.y + 1;
    const int bottom = arrow.y + arrow.height - 1;
    const int neck = bottom - arrow.headHeight;
    const int halfShaft = arrow.shaftWidth / 2;
    const std::array<POINT, 7> points{{
        { center - halfShaft, top }, { center + halfShaft, top },
        { center + halfShaft, neck }, { arrow.x + arrow.width - 1, neck },
        { center, bottom }, { arrow.x + 1, neck }, { center - halfShaft, neck }
    }};
    HBRUSH brush = CreateSolidBrush(arrow.active ? uitheme::kAccent : RGB(128, 128, 128));
    HPEN pen = CreatePen(PS_SOLID, 2, arrow.active ? RGB(57, 38, 8) : RGB(48, 48, 48));
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Polygon(hdc, points.data(), static_cast<int>(points.size()));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    const RECT highlight{ center - 1, top + 2, center + 1, neck - 1 };
    uitheme::FillRoundedRect(hdc, highlight, arrow.active ? uitheme::kAccentHover : RGB(184, 184, 184), 0);
}

void DrawLevelSelector(HDC hdc, const UISkillListWnd::DisplayLevelSelector& selector)
{
    const RECT rect{ selector.x, selector.y, selector.x + selector.width, selector.y + selector.height };
    const COLORREF color = selector.enabled ? uitheme::kAccent : RGB(96, 96, 96);
    const int radius = (std::min)(3, uitheme::kCornerRadius);
    uitheme::FillRoundedRect(hdc, rect,
        selector.enabled && selector.pressed ? uitheme::kControlHover : RGB(0, 0, 0), radius);
    uitheme::FrameRoundedRect(hdc, rect, color, radius);
    const int centerX = selector.x + selector.width / 2;
    const int centerY = selector.y + selector.height / 2;
    const int baseX = centerX - selector.direction * 2;
    const std::array<POINT, 3> points{{
        { baseX, centerY - 4 }, { centerX + selector.direction * 3, centerY }, { baseX, centerY + 4 }
    }};
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, points.data(), static_cast<int>(points.size()));
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(brush);
}

} // namespace

UISkillListWnd::UISkillListWnd()
{
    m_show = 0;
    m_w = kWindowWidth;
    m_h = kWindowHeight;
    m_bottomButtons[kBottomButtonUse].label = "Use";
    m_bottomButtons[kBottomButtonClose].label = "Close";
    m_tabs[0].label = "1st Job";
    m_tabs[1].label = "2nd Job";
    int savedX = 100;
    int savedY = 24;
    LoadUiWindowPlacement("SkillListWnd", &savedX, &savedY);
    g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
    Move(savedX, savedY);
}

UISkillListWnd::~UISkillListWnd() = default;

void UISkillListWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
    if (show != 0) {
        EnsureCreated();
        int x = m_x;
        int y = m_y;
        g_windowMgr.ClampWindowToClient(&x, &y, m_w, m_h);
        Move(x, y);
        RefreshVisibleSkillsForInteractionState();
    } else {
        m_dragArmed = false;
        m_draggingScrollThumb = false;
        m_pressedUpgradeSkillId = 0;
        m_pressedLevelSkillId = 0;
        m_pressedLevelDirection = 0;
        m_pressedTab = -1;
        m_pressedSystemButton = -1;
        for (TextButton& button : m_bottomButtons) {
            button.pressed = false;
        }
    }
}

void UISkillListWnd::Move(int x, int y)
{
    UIWindow::Move(x, y);
    LayoutChildren();
    if (m_show != 0) {
        RefreshVisibleSkillsForInteractionState();
    }
}

bool UISkillListWnd::IsUpdateNeed()
{
    return m_show != 0 && (m_isDirty != 0 || !m_hasVisualStateToken || BuildVisualStateToken() != m_lastVisualStateToken);
}

msgresult_t UISkillListWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    (void)sender;
    (void)lparam;
    (void)extra;
    if (msg != 6) {
        return 0;
    }
    if (wparam == kButtonIdClose) {
        SetShow(0);
        return 1;
    }
    return wparam == kButtonIdBase || wparam == kButtonIdMini ? 1 : 0;
}

void UISkillListWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    m_controlsCreated = true;
    LayoutChildren();
    RefreshVisibleSkillsForInteractionState();
}

void UISkillListWnd::OnDestroy()
{
    m_iconCache.clear();
}

void UISkillListWnd::OnDraw()
{
    if (m_show == 0) {
        return;
    }
    EnsureCreated();
    RefreshVisibleSkillsForInteractionState();
    if (!IsQtUiRuntimeEnabled()) {
        HDC hdc = AcquireDrawTarget();
        if (!hdc) {
            return;
        }
        HGDIOBJ oldFont = SelectObject(hdc, GetUiFont());
        DrawWindowChrome(hdc);
        DrawTextLine(hdc, m_x + 12, m_y + 2, uitheme::kUiText, "Skill Tree");
        DisplayData data;
        GetDisplayDataForQt(&data);
        for (const DisplayButton& tab : data.tabs) {
            DrawButton(hdc, tab);
        }
        SelectObject(hdc, GetUiBoldFont());
        for (const DisplayColumn& column : data.columns) {
            RECT rect{ column.x, column.y, column.x + column.width, column.y + column.height };
            DrawCenteredText(hdc, rect, column.title, uitheme::kAccent);
        }
        SelectObject(hdc, GetUiFont());
        const RECT viewport = GetSkillViewportRect();
        const int savedGridDc = SaveDC(hdc);
        if (savedGridDc != 0) {
            IntersectClipRect(hdc, viewport.left, viewport.top, viewport.right, viewport.bottom);
        }
        for (const DisplayConnection& connection : data.connections) {
            DrawSkillArrow(hdc, connection);
        }
        for (const DisplayRow& row : data.rows) {
            if (row.skillId <= 0) {
                continue;
            }
            RECT iconRect{ row.iconX, row.iconY, row.iconX + kIconSize, row.iconY + kIconSize };
            const RECT iconFrame{ iconRect.left - row.iconFrameWidth, iconRect.top - row.iconFrameWidth,
                iconRect.right + row.iconFrameWidth, iconRect.bottom + row.iconFrameWidth };
            const COLORREF frameColor = row.selected || row.hovered ? uitheme::kAccentHover : uitheme::kAccent;
            uitheme::FillRoundedRect(hdc, iconFrame, frameColor, uitheme::kCornerRadius);
            uitheme::FillRoundedRect(hdc, iconRect,
                row.selected || row.hovered ? uitheme::kControlHover : uitheme::kPanelBackground,
                (std::max)(0, uitheme::kCornerRadius - row.iconFrameWidth));
            if (row.iconVisible) {
                if (const shopui::BitmapPixels* icon = GetSkillIcon(row.skillId, !row.learned)) {
                    // Keep the original pixels even if an asset has an extra
                    // border pixel. Match QML's unscaled, clipped Image.Pad.
                    const int savedDc = SaveDC(hdc);
                    if (savedDc != 0) {
                        IntersectClipRect(hdc, iconRect.left, iconRect.top, iconRect.right, iconRect.bottom);
#if RO_PLATFORM_WINDOWS
                        if (uitheme::kCornerRadius > 0) {
                            HRGN clip = CreateRoundRectRgn(iconRect.left, iconRect.top, iconRect.right, iconRect.bottom,
                                uitheme::kCornerRadius * 2, uitheme::kCornerRadius * 2);
                            if (clip) {
                                ExtSelectClipRgn(hdc, clip, RGN_AND);
                                DeleteObject(clip);
                            }
                        }
#endif
                    }
                    RECT nativeRect{ iconRect.left + (kIconSize - icon->width) / 2,
                        iconRect.top + (kIconSize - icon->height) / 2, 0, 0 };
                    nativeRect.right = nativeRect.left + icon->width;
                    nativeRect.bottom = nativeRect.top + icon->height;
                    shopui::DrawBitmapPixelsTransparent(hdc, *icon, nativeRect);
                    if (savedDc != 0) {
                        RestoreDC(hdc, savedDc);
                    }
                }
            }
            const RECT levelRect{ row.levelX, row.levelY, row.levelX + row.levelSize, row.levelY + row.levelSize };
            uitheme::FillRoundedRect(hdc, levelRect, RGB(0, 0, 0), uitheme::kCornerRadius);
            uitheme::FrameRoundedRect(hdc, levelRect, uitheme::kAccent, uitheme::kCornerRadius);
            SelectObject(hdc, GetUiBoldFont());
            DrawCenteredText(hdc, levelRect, row.levelText, uitheme::kAccent);
            SelectObject(hdc, GetUiFont());
            if (row.levelSelectionVisible) {
                for (const DisplayLevelSelector& selector : row.levelSelectors) {
                    DrawLevelSelector(hdc, selector);
                }
            }
            if (row.upgradeVisible) {
                DisplayButton upgrade;
                upgrade.x = row.upgradeX;
                upgrade.y = row.upgradeY;
                upgrade.width = row.upgradeWidth;
                upgrade.height = row.upgradeHeight;
                upgrade.pressed = row.upgradePressed;
                upgrade.hovered = row.hovered;
                upgrade.label = "+";
                DrawButton(hdc, upgrade);
            }
        }
        if (savedGridDc != 0) {
            RestoreDC(hdc, savedGridDc);
        }
        if (data.scrollBar.visible) {
            const RECT track = GetScrollTrackRect();
            const RECT thumb = GetScrollThumbRect();
            FillRectColor(hdc, track, uitheme::kShadow);
            FrameRectColor(hdc, track, uitheme::kFrame);
            FillRectColor(hdc, thumb, m_draggingScrollThumb ? uitheme::kAccentHover : uitheme::kAccent);
        }
        DrawTextLine(hdc, m_x + 16, m_y + m_h - 26, uitheme::kAccent,
            "Skill Points: " + std::to_string(data.skillPointCount));
        for (const DisplayButton& button : data.bottomButtons) {
            DrawButton(hdc, button);
        }
        for (int index = 0; index < GetQtSystemButtonCount(); ++index) {
            QtButtonDisplay system;
            GetQtSystemButtonDisplayForQt(index, &system);
            DisplayButton button;
            button.x = system.x;
            button.y = system.y;
            button.width = system.width;
            button.height = system.height;
            button.label = system.label;
            button.pressed = m_pressedSystemButton == index;
            DrawButton(hdc, button);
        }
        SelectObject(hdc, oldFont);
        ReleaseDrawTarget(hdc);
    }
    m_lastVisualStateToken = BuildVisualStateToken();
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UISkillListWnd::OnLBtnDown(int x, int y)
{
    EnsureCreated();
    RefreshVisibleSkillsForInteractionState();
    UpdateHover(x, y);
    m_dragArmed = false;
    m_dragSkillId = 0;
    m_pressedUpgradeSkillId = 0;
    m_pressedTab = -1;
    m_pressedLevelSkillId = 0;
    m_pressedLevelDirection = 0;
    m_pressedSystemButton = -1;
    for (int index = 0; index < GetQtSystemButtonCount(); ++index) {
        QtButtonDisplay button;
        GetQtSystemButtonDisplayForQt(index, &button);
        if (IsInsideRect(MakeSkillRect(0, 0, button.x, button.y, button.width, button.height), x, y)) {
            m_pressedSystemButton = index;
            Invalidate();
            return;
        }
    }
    if (y >= m_y && y < m_y + kTitleBarHeight) {
        UIFrameWnd::OnLBtnDown(x, y);
        return;
    }
    for (int index = 0; index < kSkillTreeJobTabs; ++index) {
        if (IsInsideRect(m_tabs[index].rect, x, y)) {
            m_pressedTab = index;
            Invalidate();
            return;
        }
    }
    if (GetMaxScrollOffset() > 0 && IsInsideRect(GetScrollTrackRect(), x, y)) {
        const RECT thumb = GetScrollThumbRect();
        if (IsInsideRect(thumb, x, y)) {
            m_draggingScrollThumb = true;
            m_scrollDragOffsetY = y - thumb.top;
        } else {
            UpdateScrollFromThumbPosition(y - static_cast<int>(thumb.bottom - thumb.top) / 2);
        }
        Invalidate();
        return;
    }
    const int bottomButton = HitTestBottomButton(x, y);
    if (bottomButton >= 0) {
        m_bottomButtons[bottomButton].pressed = true;
        Invalidate();
        return;
    }
    if (m_hoveredRow >= 0) {
        const VisibleSkill& visible = m_visibleSkills[m_hoveredRow];
        if (visible.skillId <= 0) {
            return;
        }
        m_selectedSkillId = visible.skillId;
        if (CanSelectSkillLevel(visible.skillId)) {
            for (int index = 0; index < 2; ++index) {
                if (IsInsideRect(visible.levelRects[index], x, y)) {
                    const int direction = index == 0 ? -1 : 1;
                    const int level = GetSelectedSkillLevel(visible.skillId);
                    if (visible.skill && level > 0 && (direction < 0 ? level > 1 : level < visible.skill->level)) {
                        m_pressedLevelSkillId = visible.skillId;
                        m_pressedLevelDirection = direction;
                    }
                    Invalidate();
                    return;
                }
            }
        }
        if (visible.skill) {
            if (IsInsideRect(visible.upgradeRect, x, y) && visible.skill->upgradable != 0
                && g_session.GetPlayerSkillPointCount() > 0) {
                m_pressedUpgradeSkillId = visible.skillId;
            } else if (visible.skill->level > 0) {
                m_dragArmed = true;
                m_dragSkillId = visible.skillId;
                m_dragSkillLevel = GetSelectedSkillLevel(visible.skillId);
                m_dragStartPoint = POINT{ x, y };
            }
        }
        Invalidate();
    }
}

void UISkillListWnd::OnLBtnUp(int x, int y)
{
    if (m_draggingScrollThumb) {
        UpdateScrollFromThumbPosition(y - m_scrollDragOffsetY);
        m_draggingScrollThumb = false;
        Invalidate();
        return;
    }
    const bool wasDragging = m_isDragging != 0;
    UIFrameWnd::OnLBtnUp(x, y);
    RefreshVisibleSkillsForInteractionState();
    m_dragArmed = false;
    m_dragSkillId = 0;
    if (wasDragging) {
        return;
    }
    const int pressedSystemButton = m_pressedSystemButton;
    m_pressedSystemButton = -1;
    if (pressedSystemButton >= 0) {
        QtButtonDisplay button;
        GetQtSystemButtonDisplayForQt(pressedSystemButton, &button);
        if (IsInsideRect(MakeSkillRect(0, 0, button.x, button.y, button.width, button.height), x, y)) {
            SendMsg(this, 6, button.id, 0, 0);
        }
        Invalidate();
        return;
    }
    const int pressedTab = m_pressedTab;
    m_pressedTab = -1;
    if (pressedTab >= 0) {
        if (IsInsideRect(m_tabs[pressedTab].rect, x, y)) {
            SelectTab(pressedTab);
        }
        Invalidate();
        return;
    }
    const int bottomButton = HitTestBottomButton(x, y);
    for (size_t index = 0; index < m_bottomButtons.size(); ++index) {
        const bool activate = m_bottomButtons[index].pressed && static_cast<int>(index) == bottomButton;
        m_bottomButtons[index].pressed = false;
        if (activate && index == kBottomButtonClose) {
            SetShow(0);
        } else if (activate && index == kBottomButtonUse && m_selectedSkillId > 0) {
            ArmPendingSkillUseFromSkillList(m_selectedSkillId, GetSelectedSkillLevel(m_selectedSkillId));
        }
    }
    if (m_pressedLevelSkillId != 0) {
        for (const VisibleSkill& visible : m_visibleSkills) {
            const int index = m_pressedLevelDirection < 0 ? 0 : 1;
            if (visible.skillId == m_pressedLevelSkillId && IsInsideRect(GetSkillViewportRect(), x, y)
                && IsInsideRect(visible.levelRects[index], x, y)) {
                AdjustSelectedSkillLevel(m_pressedLevelSkillId, m_pressedLevelDirection);
                break;
            }
        }
    }
    m_pressedLevelSkillId = 0;
    m_pressedLevelDirection = 0;
    if (m_pressedUpgradeSkillId != 0) {
        for (const VisibleSkill& visible : m_visibleSkills) {
            if (visible.skill && visible.skillId == m_pressedUpgradeSkillId
                && IsInsideRect(GetSkillViewportRect(), x, y)
                && IsInsideRect(visible.upgradeRect, x, y) && visible.skill->upgradable != 0
                && g_session.GetPlayerSkillPointCount() > 0) {
                g_modeMgr.SendMsg(CGameMode::GameMsg_RequestUpgradeSkillLevel, m_pressedUpgradeSkillId, 0, 0);
                break;
            }
        }
    }
    m_pressedUpgradeSkillId = 0;
    Invalidate();
}

void UISkillListWnd::OnRBtnDown(int x, int y)
{
    EnsureCreated();
    RefreshVisibleSkillsForInteractionState();
    UpdateHover(x, y);
    if (m_hoveredRow < 0) {
        return;
    }
    const VisibleSkill& visible = m_visibleSkills[m_hoveredRow];
    if (visible.skill) {
        g_windowMgr.ShowSkillDescribeWindow(*visible.skill, x + 12, y + 12);
    } else if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(visible.skillId)) {
        PLAYER_SKILL_INFO skill;
        skill.SKID = visible.skillId;
        skill.skillIdName = metadata->skillIdName;
        skill.skillName = metadata->displayName;
        skill.skillMaxLv = metadata->maxLevel;
        skill.descriptionLines = metadata->descriptionLines;
        g_windowMgr.ShowSkillDescribeWindow(skill, x + 12, y + 12);
    }
}

void UISkillListWnd::OnMouseMove(int x, int y)
{
    if (m_draggingScrollThumb) {
        UpdateScrollFromThumbPosition(y - m_scrollDragOffsetY);
        return;
    }
    UIFrameWnd::OnMouseMove(x, y);
    RefreshVisibleSkillsForInteractionState();
    UpdateHover(x, y);
    if (m_dragArmed) {
        const int dx = x - m_dragStartPoint.x;
        const int dy = y - m_dragStartPoint.y;
        if (dx * dx + dy * dy >= 16) {
            if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                gameMode->m_dragType = static_cast<int>(DragType::ShortcutSkill);
                gameMode->m_dragInfo = DRAG_INFO{};
                gameMode->m_dragInfo.type = static_cast<int>(DragType::ShortcutSkill);
                gameMode->m_dragInfo.source = static_cast<int>(DragSource::SkillListWindow);
                gameMode->m_dragInfo.skillId = m_dragSkillId;
                gameMode->m_dragInfo.skillLevel = m_dragSkillLevel;
                if (g_windowMgr.m_shortCutWnd) {
                    g_windowMgr.m_shortCutWnd->Invalidate();
                }
            }
            m_dragArmed = false;
            Invalidate();
        }
    }
}

void UISkillListWnd::OnWheel(int delta)
{
    EnsureCreated();
    RefreshVisibleSkillsForInteractionState();
    if (delta != 0) {
        SetScrollOffset(m_scrollOffset + (delta > 0 ? -kRowHeight / 2 : kRowHeight / 2));
    }
}

void UISkillListWnd::StoreInfo()
{
    SaveUiWindowPlacement("SkillListWnd", m_x, m_y);
}

void UISkillListWnd::SelectTab(int index)
{
    if (index < 0 || index >= kSkillTreeJobTabs || index == m_activeTab) {
        return;
    }
    m_activeTab = index;
    m_scrollOffset = 0;
    m_draggingScrollThumb = false;
    m_selectedSkillId = 0;
    m_pressedUpgradeSkillId = 0;
    m_hoveredRow = -1;
    m_pressedLevelSkillId = 0;
    m_pressedLevelDirection = 0;
    m_dragArmed = false;
    RefreshVisibleSkillsForInteractionState();
    Invalidate();
}

bool UISkillListWnd::CanSelectSkillLevel(int skillId) const
{
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    return metadata && metadata->separateLevels;
}

int UISkillListWnd::GetSelectedSkillLevel(int skillId) const
{
    const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(skillId);
    if (!skill || skill->level <= 0) {
        return 0;
    }
    const auto selection = m_selectedSkillLevels.find(skillId);
    return CanSelectSkillLevel(skillId) && selection != m_selectedSkillLevels.end()
        ? std::clamp(selection->second, 1, skill->level) : skill->level;
}

void UISkillListWnd::AdjustSelectedSkillLevel(int skillId, int direction)
{
    const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(skillId);
    if (!skill || skill->level <= 0 || !CanSelectSkillLevel(skillId)) {
        return;
    }
    m_selectedSkillLevels[skillId] = std::clamp(GetSelectedSkillLevel(skillId) + direction, 1, skill->level);
    Invalidate();
}

RECT UISkillListWnd::GetSkillViewportRect() const
{
    return RECT{ m_x + 8, m_y + kGridTop - kKnownIconFrameWidth, m_x + m_w - 20, m_y + m_h - 48 };
}

int UISkillListWnd::GetMaxScrollOffset() const
{
    const SkillTreeLayout& tree = m_jobTrees[m_activeTab];
    for (int index = static_cast<int>(tree.skillIds.size()) - 1; index >= 0; --index) {
        if (tree.skillIds[index] > 0) {
            const int contentBottom = m_y + kGridTop + (index / kSkillTreeColumns) * kRowHeight + kCellHeight + 4;
            return (std::max)(0, contentBottom - static_cast<int>(GetSkillViewportRect().bottom));
        }
    }
    return 0;
}

RECT UISkillListWnd::GetScrollTrackRect() const
{
    const RECT viewport = GetSkillViewportRect();
    return RECT{ m_x + m_w - 15, viewport.top, m_x + m_w - 5, viewport.bottom };
}

RECT UISkillListWnd::GetScrollThumbRect() const
{
    const RECT track = GetScrollTrackRect();
    const int trackHeight = static_cast<int>(track.bottom - track.top) - 2;
    const int maxOffset = GetMaxScrollOffset();
    const int thumbHeight = (std::min)(trackHeight, (std::max)(24, trackHeight * trackHeight / (trackHeight + maxOffset)));
    const int thumbTop = static_cast<int>(track.top) + 1
        + (maxOffset > 0 ? (trackHeight - thumbHeight) * m_scrollOffset / maxOffset : 0);
    return RECT{ track.left + 1, thumbTop, track.right - 1, thumbTop + thumbHeight };
}

void UISkillListWnd::SetScrollOffset(int offset)
{
    const int nextOffset = std::clamp(offset, 0, GetMaxScrollOffset());
    if (nextOffset == m_scrollOffset) {
        return;
    }
    m_scrollOffset = nextOffset;
    m_hoveredRow = -1;
    m_pressedUpgradeSkillId = 0;
    m_dragArmed = false;
    m_pressedLevelSkillId = 0;
    m_pressedLevelDirection = 0;
    RefreshVisibleSkillsForInteractionState();
    Invalidate();
}

void UISkillListWnd::UpdateScrollFromThumbPosition(int globalY)
{
    const RECT track = GetScrollTrackRect();
    const RECT thumb = GetScrollThumbRect();
    const int minY = static_cast<int>(track.top) + 1;
    const int maxY = static_cast<int>(track.bottom) - 1 - static_cast<int>(thumb.bottom - thumb.top);
    const int travel = maxY - minY;
    if (travel > 0) {
        const int position = std::clamp(globalY, minY, maxY) - minY;
        SetScrollOffset((position * GetMaxScrollOffset() + travel / 2) / travel);
    }
}

bool UISkillListWnd::GetDisplayDataForQt(DisplayData* outData)
{
    if (!outData) {
        return false;
    }
    // Server updates can replace the session's skill list between UI draws.
    // Resolve its entries again before exposing any cached interaction data.
    RefreshVisibleSkillsForInteractionState();
    DisplayData data;
    data.skillPointCount = g_session.GetPlayerSkillPointCount();
    data.activeTab = m_activeTab;
    data.iconSize = kIconSize;
    const RECT viewport = GetSkillViewportRect();
    data.viewportX = viewport.left;
    data.viewportY = viewport.top;
    data.viewportWidth = viewport.right - viewport.left;
    data.viewportHeight = viewport.bottom - viewport.top;
    const RECT track = GetScrollTrackRect();
    const RECT thumb = GetScrollThumbRect();
    data.scrollBar.visible = GetMaxScrollOffset() > 0;
    data.scrollBar.dragging = m_draggingScrollThumb;
    data.scrollBar.x = track.left;
    data.scrollBar.y = track.top;
    data.scrollBar.width = track.right - track.left;
    data.scrollBar.height = track.bottom - track.top;
    data.scrollBar.thumbY = thumb.top;
    data.scrollBar.thumbHeight = thumb.bottom - thumb.top;
    for (int index = 0; index < kSkillTreeJobTabs; ++index) {
        const TextButton& tab = m_tabs[index];
        data.tabs.push_back({ static_cast<int>(tab.rect.left), static_cast<int>(tab.rect.top),
            static_cast<int>(tab.rect.right - tab.rect.left), static_cast<int>(tab.rect.bottom - tab.rect.top),
            tab.hovered, index == m_pressedTab, index == m_activeTab, tab.label });
    }
    const SkillTreeLayout& tree = m_jobTrees[m_activeTab];
    for (int column = 0; column < kSkillTreeColumns; ++column) {
        data.columns.push_back({ m_x + kGridLeft + column * (kColumnWidth + kColumnGap), m_y + 56,
            kColumnWidth, 22, tree.titles[column] });
    }
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    const bool hideDraggedSkill = gameMode
        && gameMode->m_dragType == static_cast<int>(DragType::ShortcutSkill)
        && gameMode->m_dragInfo.source == static_cast<int>(DragSource::SkillListWindow);
    std::array<int, kSkillTreeColumns> previousIconBottom{};
    std::array<int, kSkillTreeColumns> previousSkillLevel{};
    for (size_t index = 0; index < m_visibleSkills.size(); ++index) {
        const VisibleSkill& visible = m_visibleSkills[index];
        const PLAYER_SKILL_INFO* skill = visible.skill;
        const RECT& cell = visible.rowRect;
        const RECT icon = GetIconRect(cell);
        DisplayRow row;
        row.skillId = visible.skillId;
        row.x = cell.left;
        row.y = cell.top;
        row.width = cell.right - cell.left;
        row.height = cell.bottom - cell.top;
        row.iconX = icon.left;
        row.iconY = icon.top;
        row.learned = skill && skill->level >= 1;
        row.iconFrameWidth = row.learned ? kKnownIconFrameWidth : kUnknownIconFrameWidth;
        row.levelX = icon.right - kLevelBadgeOverlap;
        row.levelY = icon.bottom - kLevelBadgeOverlap;
        row.levelSize = kLevelBadgeSize;
        row.levelSelectionVisible = CanSelectSkillLevel(visible.skillId);
        const int selectedLevel = GetSelectedSkillLevel(visible.skillId);
        for (int control = 0; control < 2; ++control) {
            const RECT& rect = visible.levelRects[control];
            DisplayLevelSelector& selector = row.levelSelectors[control];
            selector.x = rect.left;
            selector.y = rect.top;
            selector.width = rect.right - rect.left;
            selector.height = rect.bottom - rect.top;
            selector.direction = control == 0 ? -1 : 1;
            selector.enabled = row.levelSelectionVisible && skill && selectedLevel > 0
                && (control == 0 ? selectedLevel > 1 : selectedLevel < skill->level);
            selector.pressed = selector.enabled && m_pressedLevelSkillId == visible.skillId
                && m_pressedLevelDirection == selector.direction;
        }
        row.iconVisible = !(hideDraggedSkill && visible.skillId == gameMode->m_dragInfo.skillId);
        row.selected = visible.skillId > 0 && visible.skillId == m_selectedSkillId;
        row.hovered = visible.skillId > 0 && static_cast<int>(index) == m_hoveredRow;
        row.upgradeVisible = skill && skill->upgradable != 0 && data.skillPointCount > 0;
        row.upgradePressed = visible.skillId > 0 && visible.skillId == m_pressedUpgradeSkillId;
        row.upgradeX = visible.upgradeRect.left;
        row.upgradeY = visible.upgradeRect.top;
        row.upgradeWidth = visible.upgradeRect.right - visible.upgradeRect.left;
        row.upgradeHeight = visible.upgradeRect.bottom - visible.upgradeRect.top;
        if (visible.skillId > 0) {
            row.levelText = std::to_string(selectedLevel);
            const int column = static_cast<int>(index) % kSkillTreeColumns;
            if (previousIconBottom[column] != 0) {
                DisplayConnection arrow;
                arrow.x = icon.left + kIconSize / 2 - kArrowWidth / 2;
                arrow.y = previousIconBottom[column] + kArrowGap;
                arrow.width = kArrowWidth;
                arrow.height = icon.top - row.iconFrameWidth - kArrowGap - arrow.y;
                arrow.shaftWidth = kArrowShaftWidth;
                arrow.headHeight = kArrowHeadHeight;
                arrow.active = previousSkillLevel[column] >= 3;
                if (arrow.height > kArrowHeadHeight + 2) {
                    data.connections.push_back(arrow);
                }
            }
            previousIconBottom[column] = icon.bottom + row.iconFrameWidth;
            previousSkillLevel[column] = skill ? skill->level : 0;
        }
        data.rows.push_back(std::move(row));
    }
    for (const TextButton& button : m_bottomButtons) {
        data.bottomButtons.push_back({ static_cast<int>(button.rect.left), static_cast<int>(button.rect.top),
            static_cast<int>(button.rect.right - button.rect.left), static_cast<int>(button.rect.bottom - button.rect.top),
            button.hovered, button.pressed, false, button.label });
    }
    *outData = std::move(data);
    return true;
}

int UISkillListWnd::GetQtSystemButtonCount() const
{
    return 3;
}

bool UISkillListWnd::GetQtSystemButtonDisplayForQt(int index, QtButtonDisplay* outData) const
{
    if (!outData || index < 0 || index >= GetQtSystemButtonCount()) {
        return false;
    }
    static const std::array<int, 3> ids{ kButtonIdBase, kButtonIdMini, kButtonIdClose };
    static const std::array<const char*, 3> labels{ "B", "_", "X" };
    outData->id = ids[index];
    outData->x = m_x + m_w - 49 + index * 16;
    outData->y = m_y + 2;
    outData->width = kQtButtonWidth;
    outData->height = kQtButtonHeight;
    outData->label = labels[index];
    outData->visible = true;
    return true;
}

void UISkillListWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        Create(m_w, m_h);
        OnCreate(m_x, m_y);
    }
}

void UISkillListWnd::LayoutChildren()
{
    for (int index = 0; index < kSkillTreeJobTabs; ++index) {
        const int x = m_x + kGridLeft + index * 116;
        m_tabs[index].rect = RECT{ x, m_y + 26, x + 108, m_y + 50 };
    }
    const int bottomY = m_y + m_h - 32;
    m_bottomButtons[kBottomButtonClose].rect = RECT{ m_x + m_w - 80, bottomY, m_x + m_w - 16, bottomY + 24 };
    m_bottomButtons[kBottomButtonUse].rect = RECT{ m_x + m_w - 152, bottomY, m_x + m_w - 88, bottomY + 24 };
}

void UISkillListWnd::RefreshVisibleSkillsForInteractionState()
{
    if (m_levelSelectionCharacterId != g_session.m_gid) {
        m_selectedSkillLevels.clear();
        m_levelSelectionCharacterId = g_session.m_gid;
        m_pressedLevelSkillId = 0;
        m_pressedLevelDirection = 0;
        m_dragArmed = false;
    }
    for (auto selection = m_selectedSkillLevels.begin(); selection != m_selectedSkillLevels.end();) {
        const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(selection->first);
        if (!skill || skill->level <= 0) {
            selection = m_selectedSkillLevels.erase(selection);
        } else {
            selection->second = std::clamp(selection->second, 1, skill->level);
            ++selection;
        }
    }
    if (m_layoutJobId != g_session.m_playerJob) {
        m_jobTrees = g_skillMgr.GetSkillTreeTabs(g_session.m_playerJob);
        m_layoutJobId = g_session.m_playerJob;
        m_activeTab = 0;
        m_scrollOffset = 0;
        m_draggingScrollThumb = false;
        m_selectedSkillId = 0;
        m_pressedUpgradeSkillId = 0;
        m_pressedLevelSkillId = 0;
        m_pressedLevelDirection = 0;
        m_dragArmed = false;
        m_hoveredRow = -1;
    }
    const SkillTreeLayout& tree = m_jobTrees[m_activeTab];
    m_scrollOffset = std::clamp(m_scrollOffset, 0, GetMaxScrollOffset());
    m_visibleSkills.clear();
    for (int row = 0; row < kSkillTreeRows; ++row) {
        for (int column = 0; column < kSkillTreeColumns; ++column) {
            const int skillId = tree.skillIds[row * kSkillTreeColumns + column];
            const int left = m_x + kGridLeft + column * (kColumnWidth + kColumnGap);
            const int top = m_y + kGridTop + row * kRowHeight - m_scrollOffset;
            const RECT cell{ left, top, left + kColumnWidth, top + kCellHeight };
            const RECT upgrade{ cell.right - 25, cell.top + 4, cell.right - 5, cell.top + 24 };
            const PLAYER_SKILL_INFO* skill = skillId > 0 ? g_session.GetSkillItemBySkillId(skillId) : nullptr;
            VisibleSkill visible;
            visible.skillId = skillId;
            visible.skill = skill;
            visible.rowRect = cell;
            visible.upgradeRect = upgrade;
            visible.levelRects[0] = GetLevelSelectorRect(cell, -1);
            visible.levelRects[1] = GetLevelSelectorRect(cell, 1);
            m_visibleSkills.push_back(visible);
        }
    }
}

void UISkillListWnd::UpdateHover(int globalX, int globalY)
{
    const int oldHoveredRow = m_hoveredRow;
    m_hoveredRow = -1;
    for (size_t index = 0; index < m_visibleSkills.size(); ++index) {
        if (IsInsideRect(GetSkillViewportRect(), globalX, globalY)
            && IsInsideRect(m_visibleSkills[index].rowRect, globalX, globalY)) {
            m_hoveredRow = static_cast<int>(index);
            break;
        }
    }
    if (oldHoveredRow != m_hoveredRow) {
        Invalidate();
    }
    const auto updateButtons = [this, globalX, globalY](auto& buttons) {
        for (TextButton& button : buttons) {
            const bool hovered = IsInsideRect(button.rect, globalX, globalY);
            if (button.hovered != hovered) {
                button.hovered = hovered;
                Invalidate();
            }
        }
    };
    updateButtons(m_tabs);
    updateButtons(m_bottomButtons);
}

void UISkillListWnd::DrawWindowChrome(HDC hdc) const
{
    const RECT outer{ m_x, m_y, m_x + m_w, m_y + m_h };
    FillRectColor(hdc, outer, uitheme::kPanelBackground);
    FrameRectColor(hdc, outer, uitheme::kFrame);
    const RECT titleRule{ m_x + 8, m_y + kTitleBarHeight + 2, m_x + m_w - 8, m_y + kTitleBarHeight + 3 };
    FillRectColor(hdc, titleRule, uitheme::kFrame);
    const RECT footerRule{ m_x + 8, m_y + m_h - 40, m_x + m_w - 8, m_y + m_h - 39 };
    FillRectColor(hdc, footerRule, uitheme::kFrame);
}

void UISkillListWnd::DrawButton(HDC hdc, const DisplayButton& button) const
{
    const RECT rect{ button.x, button.y, button.x + button.width, button.y + button.height };
    FillRectColor(hdc, rect, button.pressed || button.hovered || button.selected
        ? uitheme::kControlHover : uitheme::kPanelBackground);
    FrameRectColor(hdc, rect, button.selected ? uitheme::kAccent : uitheme::kFrame);
    DrawCenteredText(hdc, rect, button.label, uitheme::kUiText);
}

int UISkillListWnd::HitTestBottomButton(int globalX, int globalY) const
{
    for (size_t index = 0; index < m_bottomButtons.size(); ++index) {
        if (IsInsideRect(m_bottomButtons[index].rect, globalX, globalY)) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

const shopui::BitmapPixels* UISkillListWnd::GetSkillIcon(int skillId, bool grayscale)
{
    const unsigned long long cacheKey = (static_cast<unsigned long long>(skillId) << 1) | (grayscale ? 1ull : 0ull);
    const auto existing = m_iconCache.find(cacheKey);
    if (existing != m_iconCache.end()) {
        return existing->second.IsValid() ? &existing->second : nullptr;
    }
    const std::string path = g_skillMgr.GetSkillTreeIconPath(skillId);
    shopui::BitmapPixels bitmap;
    if (!path.empty()) {
        bitmap = shopui::LoadBitmapPixelsFromGameData(path, true);
        // Never upscale the legacy 24px artwork into a skill-tree cell.
        if (bitmap.width < kIconSize || bitmap.height < kIconSize) {
            bitmap.Clear();
        }
        if (grayscale) {
            shopui::ConvertBitmapPixelsToGrayscale(bitmap);
        }
    }
    const auto inserted = m_iconCache.emplace(cacheKey, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

unsigned long long UISkillListWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(m_hoveredRow)));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_activeTab));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_scrollOffset));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_draggingScrollThumb));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedTab));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedSystemButton));
    HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_playerJob));
    for (const TextButton& button : m_tabs) {
        HashTokenValue(&hash, button.hovered);
    }
    for (const TextButton& button : m_bottomButtons) {
        HashTokenValue(&hash, button.hovered);
        HashTokenValue(&hash, button.pressed);
    }
    HashTokenValue(&hash, static_cast<unsigned long long>(m_selectedSkillId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedUpgradeSkillId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedLevelSkillId));
    HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedLevelDirection));
    HashTokenValue(&hash, static_cast<unsigned long long>(g_session.m_gid));
    for (const auto& selection : m_selectedSkillLevels) {
        HashTokenValue(&hash, static_cast<unsigned long long>(selection.first));
        HashTokenValue(&hash, static_cast<unsigned long long>(selection.second));
    }
    HashTokenValue(&hash, static_cast<unsigned long long>(g_session.GetPlayerSkillPointCount()));
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.skillId));
        HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.skillLevel));
    }

    const std::list<PLAYER_SKILL_INFO>& skillItems = g_session.GetSkillItems();
    HashTokenValue(&hash, static_cast<unsigned long long>(skillItems.size()));
    for (const PLAYER_SKILL_INFO& skill : skillItems) {
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.m_isValid));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.SKID));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.type));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.level));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.spcost));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.upgradable));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.attackRange));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.skillPos));
        HashTokenValue(&hash, static_cast<unsigned long long>(skill.skillMaxLv));
        HashTokenString(&hash, skill.skillIdName);
        HashTokenString(&hash, skill.skillName);
    }

    return hash;
}
