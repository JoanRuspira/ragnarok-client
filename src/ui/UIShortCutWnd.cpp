#include "UIShortCutWnd.h"
#include "ui/UiTheme.h"

#include "UIShopCommon.h"
#include "UINewChatWnd.h"
#include "UIWindowMgr.h"
#include "UiScale.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "item/Item.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "session/Session.h"
#include "skill/Skill.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QString>
#endif

#include <algorithm>
#include <string>

namespace {

constexpr int kDefaultWindowWidth = UIShortCutWnd::kVisibleSlotCount * UIShortCutWnd::kSlotSize + 2 * UIShortCutWnd::kSlotPadding;
constexpr int kDefaultWindowHeight = UIShortCutWnd::kSlotSize + 2 * UIShortCutWnd::kSlotPadding;
constexpr int kChatGap = 4;
constexpr int kSlotLabelFontSize = 18;

void DrawShortcutPanel(HDC hdc, const RECT& rect)
{
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    if (!hdc || width <= 0 || height <= 0) {
        return;
    }

    // Match chat's 75% stippled coverage. This also works with the legacy
    // overlay's color-key transparency, which cannot retain partial alpha.
    static shopui::BitmapPixels background;
    if (background.width != width || background.height != height) {
        background.width = width;
        background.height = height;
        background.pixels.resize(static_cast<size_t>(width) * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                background.pixels[static_cast<size_t>(y) * width + x] = ((x + y) % 4) == 0 ? 0u : 0xFF000000u;
            }
        }
    }
    shopui::DrawBitmapPixelsTransparent(hdc, background, rect);
}

HFONT GetShortcutLabelFont()
{
#if RO_PLATFORM_WINDOWS
    static HFONT font = CreateFontA(-kSlotLabelFontSize, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        NONANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    return font ? font : shopui::GetUiFont();
#else
    return shopui::GetUiFont();
#endif
}

void DrawBitmapFit(HDC hdc, const shopui::BitmapPixels& bitmap, const RECT& rect)
{
    shopui::DrawBitmapPixelsTransparent(hdc, bitmap, rect);
}

#if RO_ENABLE_QT6_UI
QFont BuildShortCutFontFromHdc(HDC hdc)
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
    font.setPixelSize(kSlotLabelFontSize);
    font.setBold(true);
    font.setStyleStrategy(QFont::NoAntialias);
    return font;
}

void DrawShortCutTextPass(HDC hdc, const RECT& rect, const QString& text, COLORREF color)
{
    if (!hdc || text.isEmpty() || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

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
    painter.setFont(BuildShortCutFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), Qt::AlignCenter, text);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawOutlinedText(HDC hdc, RECT rect, const std::string& text, COLORREF fillColor, COLORREF outlineColor = RGB(0, 0, 0))
{
    if (!hdc || text.empty()) {
        return;
    }

    SetBkMode(hdc, TRANSPARENT);
    HGDIOBJ oldFont = SelectObject(hdc, GetShortcutLabelFont());

#if RO_ENABLE_QT6_UI
    const QString label = QString::fromLocal8Bit(text.c_str());
    RECT shadowRect = rect;
    OffsetRect(&shadowRect, -1, 0);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 1, 0);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, -1);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, 1);
    DrawShortCutTextPass(hdc, shadowRect, label, outlineColor);
    DrawShortCutTextPass(hdc, rect, label, fillColor);
#else
    // DrawText only supports DT_VCENTER for a single line. Center the measured
    // text block explicitly so the key label and optional count stay together.
    RECT measured = rect;
    DrawTextA(hdc, text.c_str(), -1, &measured, DT_CALCRECT | DT_CENTER | DT_NOPREFIX);
    const int textHeight = measured.bottom - measured.top;
    rect.top += (std::max)(0, static_cast<int>(rect.bottom - rect.top - textHeight) / 2);
    rect.bottom = rect.top + textHeight;
    RECT shadowRect = rect;
    SetTextColor(hdc, outlineColor);
    OffsetRect(&shadowRect, -1, 0);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_CENTER | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 1, 0);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_CENTER | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, -1);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_CENTER | DT_NOPREFIX);
    shadowRect = rect;
    OffsetRect(&shadowRect, 0, 1);
    DrawTextA(hdc, text.c_str(), -1, &shadowRect, DT_CENTER | DT_NOPREFIX);

    SetTextColor(hdc, fillColor);
    DrawTextA(hdc, text.c_str(), -1, &rect, DT_CENTER | DT_NOPREFIX);
#endif
    SelectObject(hdc, oldFont);
}

std::string ResolveShortcutSkillIconPath(int skillId)
{
    g_skillMgr.EnsureLoaded();
    const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId);
    if (metadata && !metadata->skillIdName.empty()) {
        const std::string lowered = shopui::ToLowerAscii(metadata->skillIdName);
        const std::string direct = "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\item\\" + lowered + ".bmp";
        const std::string dataPath = "data\\" + direct;
        if (g_fileMgr.IsDataExist(direct.c_str())) {
            return direct;
        }
        if (g_fileMgr.IsDataExist(dataPath.c_str())) {
            return dataPath;
        }
        return direct;
    }
    return g_skillMgr.GetSkillIconPath(skillId);
}

std::string BuildShortcutSlotKeyLabel(int visibleSlot)
{
    if (visibleSlot < 0 || visibleSlot >= UIShortCutWnd::kVisibleSlotCount) {
        return std::string();
    }
    return "F" + std::to_string(visibleSlot + 1);
}

int ResolveShortcutSkillUseLevel(const SHORTCUT_SLOT& slot, const PLAYER_SKILL_INFO* skill)
{
    if (slot.count > 0) {
        return static_cast<int>(slot.count);
    }
    if (skill && skill->level > 0) {
        return skill->level;
    }
    return 1;
}

int ResolveShortcutSkillSpCost(int skillId, int useLevel, const PLAYER_SKILL_INFO* skill)
{
    if (useLevel <= 0) {
        return 0;
    }

    g_skillMgr.EnsureLoaded();
    if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId)) {
        const size_t levelIndex = static_cast<size_t>(useLevel - 1);
        if (levelIndex < metadata->levelSpCosts.size()) {
            return metadata->levelSpCosts[levelIndex];
        }
    }

    if (skill && skill->spcost > 0) {
        return skill->spcost;
    }

    return 0;
}

std::string ResolveShortcutSkillName(int skillId, const PLAYER_SKILL_INFO* skill)
{
    if (skill) {
        if (!skill->skillName.empty()) {
            return skill->skillName;
        }
        if (!skill->skillIdName.empty()) {
            return skill->skillIdName;
        }
    }

    g_skillMgr.EnsureLoaded();
    if (const SkillMetadata* metadata = g_skillMgr.GetSkillMetadata(skillId)) {
        if (!metadata->displayName.empty()) {
            return metadata->displayName;
        }
        if (!metadata->skillIdName.empty()) {
            return metadata->skillIdName;
        }
    }

    return "Skill " + std::to_string(skillId);
}

std::string BuildShortcutSkillHoverText(int visibleSlot, const SHORTCUT_SLOT& slot)
{
    if (slot.id == 0 || slot.isSkill == 0) {
        return std::string();
    }

    const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot.id));
    const std::string keyLabel = BuildShortcutSlotKeyLabel(visibleSlot);
    const std::string skillName = ResolveShortcutSkillName(static_cast<int>(slot.id), skill);
    const int useLevel = ResolveShortcutSkillUseLevel(slot, skill);
    const int spCost = ResolveShortcutSkillSpCost(static_cast<int>(slot.id), useLevel, skill);

    std::string text = keyLabel;
    if (!text.empty()) {
        text += " ";
    }
    text += skillName;
    text += " Use Lv ";
    text += std::to_string(useLevel);
    text += " (Sp: ";
    text += std::to_string(spCost);
    text += ")";
    return text;
}

std::string BuildShortcutItemHoverText(int visibleSlot, const ITEM_INFO& item)
{
    const std::string keyLabel = BuildShortcutSlotKeyLabel(visibleSlot);
    const std::string itemText = shopui::BuildItemHoverText(item);
    if (keyLabel.empty()) {
        return itemText;
    }
    if (itemText.empty()) {
        return keyLabel;
    }
    return keyLabel + " " + itemText;
}

} // namespace

UIShortCutWnd::UIShortCutWnd()
    : m_hoverSlot(-1),
      m_pressedSlot(-1),
      m_pressedSlotAbsoluteIndex(-1),
      m_slotPressArmed(0),
      m_isDragging(0),
      m_startGlobalX(0),
      m_startGlobalY(0),
      m_orgX(0),
      m_orgY(0),
      m_lastDrawStateToken(0ull),
      m_hasDrawStateToken(false)
{
    Create(kDefaultWindowWidth, kDefaultWindowHeight);
    g_session.SetShortcutPage(0);

    RECT clientRect{ 0, 0, WINDOW_WIDTH > 0 ? WINDOW_WIDTH : 640, WINDOW_HEIGHT > 0 ? WINDOW_HEIGHT : 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }
    const bool scaledUi = IsQtUiRuntimeEnabled();
    const int clientWidth = (std::max)(1, scaledUi
        ? UiScaleRawToLogicalCoordinate(clientRect.right - clientRect.left)
        : static_cast<int>(clientRect.right - clientRect.left));
    const int clientHeight = (std::max)(1, scaledUi
        ? UiScaleRawToLogicalCoordinate(clientRect.bottom - clientRect.top)
        : static_cast<int>(clientRect.bottom - clientRect.top));

    // Start beside chat on the bottom edge each time. Dragging remains available
    // for the current window's lifetime, as it does for chat.
    const int maxX = (std::max)(0, clientWidth - m_w);
    int x = 0;
    int y = (std::max)(0, clientHeight - m_h);
    const UINewChatWnd* chat = g_windowMgr.m_chatWnd;
    if (chat && chat->m_show != 0) {
        x = chat->m_x + chat->m_w + kChatGap;
        if (x > maxX) {
            // Stack above chat when the client is too narrow for both windows.
            x = std::clamp(chat->m_x, 0, maxX);
            y = (std::max)(0, chat->m_y - m_h - kChatGap);
        }
    }
    Move(x, y);
}

UIShortCutWnd::~UIShortCutWnd() = default;

void UIShortCutWnd::SetShow(int show)
{
    UIWindow::SetShow(show);
}

bool UIShortCutWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasDrawStateToken) {
        return true;
    }
    return BuildDisplayStateToken() != m_lastDrawStateToken;
}

void UIShortCutWnd::DragAndDrop(int x, int y, const DRAG_INFO* const info)
{
    if (!info) {
        return;
    }

    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot < 0) {
        return;
    }

    bool changed = false;
    if (info->source == static_cast<int>(DragSource::ShortcutWindow)
        && info->shortcutSlotAbsoluteIndex >= 0) {
        const int targetAbsoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
        if (targetAbsoluteSlot == info->shortcutSlotAbsoluteIndex) {
            return;
        }

        const SHORTCUT_SLOT* sourceSlot = g_session.GetShortcutSlotByAbsoluteIndex(info->shortcutSlotAbsoluteIndex);
        if (!sourceSlot || sourceSlot->id == 0) {
            return;
        }

        changed = g_session.SetShortcutSlotByAbsoluteIndex(
            targetAbsoluteSlot,
            sourceSlot->isSkill,
            sourceSlot->id,
            sourceSlot->count);
        const bool cleared = g_session.ClearShortcutSlotByAbsoluteIndex(info->shortcutSlotAbsoluteIndex);
        if (changed) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, targetAbsoluteSlot, 0, 0);
        }
        if (cleared) {
            g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, info->shortcutSlotAbsoluteIndex, 0, 0);
        }
        if (changed || cleared) {
            Invalidate();
        }
        return;
    }

    if (info->type == static_cast<int>(DragType::ShortcutItem) && info->itemId != 0) {
        changed = g_session.SetShortcutSlotByVisibleIndex(visibleSlot, 0, info->itemId, 0);
    } else if (info->type == static_cast<int>(DragType::ShortcutSkill) && info->skillId != 0) {
        const unsigned short skillLevel = static_cast<unsigned short>((std::max)(1, info->skillLevel));
        changed = g_session.SetShortcutSlotByVisibleIndex(visibleSlot, 1, static_cast<unsigned int>(info->skillId), skillLevel);
    }

    if (changed) {
        const int absoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
        g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUpdate, absoluteSlot, 0, 0);
        Invalidate();
    }
}

void UIShortCutWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawStateToken = BuildDisplayStateToken();
        m_hasDrawStateToken = true;
        m_isDirty = 0;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const RECT bounds{ m_x, m_y, m_x + m_w, m_y + m_h };
    DrawShortcutPanel(hdc, bounds);
    shopui::FrameRectColor(hdc, bounds, RGB(96, 96, 96));

    for (int slotIndex = 0; slotIndex < kVisibleSlotCount; ++slotIndex) {
        const RECT slotRect = GetSlotRect(slotIndex);
        shopui::FrameRectColor(hdc, slotRect, m_hoverSlot == slotIndex ? RGB(192, 192, 192) : RGB(96, 96, 96));

        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        if (!slot || slot->id == 0) {
            continue;
        }

        const int absoluteSlot = g_session.GetShortcutSlotAbsoluteIndex(slotIndex);
        const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
        if (gameMode
            && gameMode->m_dragType != static_cast<int>(DragType::None)
            && gameMode->m_dragInfo.source == static_cast<int>(DragSource::ShortcutWindow)
            && gameMode->m_dragInfo.shortcutSlotAbsoluteIndex == absoluteSlot) {
            continue;
        }

        RECT iconRect = slotRect;
        ++iconRect.left;
        ++iconRect.top;
        --iconRect.right;
        --iconRect.bottom;
        if (slot->isSkill != 0) {
            if (const shopui::BitmapPixels* icon = GetSkillIcon(static_cast<int>(slot->id))) {
                DrawBitmapFit(hdc, *icon, iconRect);
            }
        } else {
            ITEM_INFO fallbackItem{};
            fallbackItem.SetItemId(slot->id);
            fallbackItem.m_isIdentified = 1;
            const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
            const ITEM_INFO& iconSource = item ? *item : fallbackItem;
            if (const shopui::BitmapPixels* icon = GetItemIcon(iconSource)) {
                DrawBitmapFit(hdc, *icon, iconRect);
            }
        }
    }

    for (int slotIndex = 0; slotIndex < kVisibleSlotCount; ++slotIndex) {
        DrawOutlinedText(hdc, GetSlotRect(slotIndex), GetSlotDisplayText(slotIndex), RGB(255, 255, 255));
    }

    ReleaseDrawTarget(hdc);

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIShortCutWnd::OnLBtnDown(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot >= 0) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(visibleSlot);
        if (slot && slot->id != 0) {
            m_pressedSlot = visibleSlot;
            m_pressedSlotAbsoluteIndex = g_session.GetShortcutSlotAbsoluteIndex(visibleSlot);
            m_slotPressArmed = 1;
            m_startGlobalX = x;
            m_startGlobalY = y;
        }
        return;
    }

    m_pressedSlot = -1;
    m_pressedSlotAbsoluteIndex = -1;
    m_slotPressArmed = 0;

    const RECT bounds{ m_x, m_y, m_x + m_w, m_y + m_h };
    if (x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom) {
        m_isDragging = 1;
        m_startGlobalX = x;
        m_startGlobalY = y;
        m_orgX = m_x;
        m_orgY = m_y;
    }
}

void UIShortCutWnd::OnLBtnDblClk(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot >= 0) {
        ActivateSlot(visibleSlot);
    }
}

void UIShortCutWnd::OnLBtnUp(int x, int y)
{
    if (m_slotPressArmed != 0
        && m_pressedSlot >= 0
        && GetSlotAtGlobalPoint(x, y) == m_pressedSlot) {
        ActivateSlot(m_pressedSlot);
    }
    m_pressedSlot = -1;
    m_pressedSlotAbsoluteIndex = -1;
    m_slotPressArmed = 0;

    m_isDragging = 0;
}

void UIShortCutWnd::OnMouseMove(int x, int y)
{
    if (m_isDragging != 0) {
        int snappedX = m_orgX + (x - m_startGlobalX);
        int snappedY = m_orgY + (y - m_startGlobalY);
        g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
        g_windowMgr.ClampWindowToClient(&snappedX, &snappedY, m_w, m_h);
        Move(snappedX, snappedY);
        return;
    }

    if (m_slotPressArmed != 0 && m_pressedSlotAbsoluteIndex >= 0) {
        const int dx = x - m_startGlobalX;
        const int dy = y - m_startGlobalY;
        if ((dx * dx) + (dy * dy) >= 16) {
            const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByAbsoluteIndex(m_pressedSlotAbsoluteIndex);
            if (slot && slot->id != 0) {
                if (CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
                    gameMode->m_dragType = static_cast<int>(slot->isSkill != 0 ? DragType::ShortcutSkill : DragType::ShortcutItem);
                    gameMode->m_dragInfo = DRAG_INFO{};
                    gameMode->m_dragInfo.type = gameMode->m_dragType;
                    gameMode->m_dragInfo.source = static_cast<int>(DragSource::ShortcutWindow);
                    gameMode->m_dragInfo.shortcutSlotAbsoluteIndex = m_pressedSlotAbsoluteIndex;
                    if (slot->isSkill != 0) {
                        gameMode->m_dragInfo.skillId = static_cast<int>(slot->id);
                        gameMode->m_dragInfo.skillLevel = static_cast<int>(slot->count);
                    } else {
                        gameMode->m_dragInfo.itemId = slot->id;
                    }
                    Invalidate();
                }
            }
            m_slotPressArmed = 0;
        }
    }

    UpdateHoverSlot(x, y);
}

void UIShortCutWnd::OnMouseHover(int x, int y)
{
    UpdateHoverSlot(x, y);
}

void UIShortCutWnd::OnRBtnDown(int x, int y)
{
    const int visibleSlot = GetSlotAtGlobalPoint(x, y);
    if (visibleSlot < 0) {
        return;
    }

    const SHORTCUT_SLOT* const slot = g_session.GetShortcutSlotByVisibleIndex(visibleSlot);
    if (!slot || slot->id == 0) {
        return;
    }

    if (slot->isSkill != 0) {
        const PLAYER_SKILL_INFO* const skill = g_session.GetSkillItemBySkillId(static_cast<int>(slot->id));
        if (skill) {
            g_windowMgr.ShowSkillDescribeWindow(*skill, x + 12, y + 12);
        }
        return;
    }

    ITEM_INFO fallbackItem{};
    fallbackItem.SetItemId(slot->id);
    fallbackItem.m_isIdentified = 1;
    const ITEM_INFO* const item = g_session.GetInventoryItemByItemId(slot->id);
    g_windowMgr.ShowItemInfoWindow(item ? *item : fallbackItem, x + 12, y + 12);
}

int UIShortCutWnd::GetHoverSlot() const
{
    return m_hoverSlot;
}

bool UIShortCutWnd::GetHoveredItemForQt(shopui::ItemHoverInfo* outData) const
{
    if (!outData || m_show == 0 || m_hoverSlot < 0) {
        return false;
    }

    const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(m_hoverSlot);
    if (!slot) {
        return false;
    }

    outData->anchorRect = GetSlotRect(m_hoverSlot);
    outData->itemId = 0;
    outData->identified = true;

    if (slot->id == 0) {
        outData->text = BuildShortcutSlotKeyLabel(m_hoverSlot);
        return outData->IsValid();
    }

    if (slot->isSkill != 0) {
        outData->text = BuildShortcutSkillHoverText(m_hoverSlot, *slot);
        return outData->IsValid();
    }

    ITEM_INFO fallbackItem{};
    fallbackItem.SetItemId(slot->id);
    fallbackItem.m_isIdentified = 1;
    const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id);
    const ITEM_INFO& hoverItem = item ? *item : fallbackItem;

    outData->text = BuildShortcutItemHoverText(m_hoverSlot, hoverItem);
    outData->itemId = hoverItem.GetItemId();
    outData->identified = hoverItem.m_isIdentified != 0;
    return outData->IsValid();
}

void UIShortCutWnd::UpdateHoverSlot(int globalX, int globalY)
{
    const int newHoverSlot = GetSlotAtGlobalPoint(globalX, globalY);
    if (m_hoverSlot != newHoverSlot) {
        m_hoverSlot = newHoverSlot;
        Invalidate();
    }
}

int UIShortCutWnd::GetSlotAtGlobalPoint(int globalX, int globalY) const
{
    for (int slotIndex = 0; slotIndex < kVisibleSlotCount; ++slotIndex) {
        const RECT rect = GetSlotRect(slotIndex);
        if (globalX >= rect.left && globalX < rect.right
            && globalY >= rect.top && globalY < rect.bottom) {
            return slotIndex;
        }
    }
    return -1;
}

RECT UIShortCutWnd::GetSlotRect(int visibleSlot) const
{
    if (visibleSlot < 0 || visibleSlot >= kVisibleSlotCount) {
        return RECT{};
    }
    return RECT{
        m_x + kSlotPadding + visibleSlot * kSlotSize,
        m_y + kSlotPadding,
        m_x + kSlotPadding + (visibleSlot + 1) * kSlotSize,
        m_y + kSlotPadding + kSlotSize
    };
}

void UIShortCutWnd::ActivateSlot(int visibleSlot)
{
    g_modeMgr.SendMsg(CGameMode::GameMsg_RequestShortcutUse, visibleSlot, 0, 0);
}

std::string UIShortCutWnd::GetSlotDisplayText(int visibleSlot) const
{
    std::string text = BuildShortcutSlotKeyLabel(visibleSlot);
    if (text.empty()) {
        return text;
    }

    const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(visibleSlot);
    if (!slot || slot->id == 0) {
        return text;
    }

    if (slot->isSkill != 0) {
        return {};
    }

    int value = 0;
    if (const ITEM_INFO* item = g_session.GetInventoryItemByItemId(slot->id)) {
        value = item->m_num;
    }
    if (value > 0) {
        text += '\n';
        text += std::to_string(value);
    }
    return text;
}

const shopui::BitmapPixels* UIShortCutWnd::GetItemIcon(const ITEM_INFO& item)
{
    const unsigned int itemId = item.GetItemId();
    const auto found = m_itemIconCache.find(itemId);
    if (found != m_itemIconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    for (const std::string& candidate : shopui::BuildItemIconCandidates(item)) {
        if (!g_fileMgr.IsDataExist(candidate.c_str())) {
            continue;
        }
        bitmap = shopui::LoadBitmapPixelsFromGameData(candidate, true);
        if (bitmap.IsValid()) {
            break;
        }
    }

    auto inserted = m_itemIconCache.emplace(itemId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

const shopui::BitmapPixels* UIShortCutWnd::GetSkillIcon(int skillId)
{
    const auto found = m_skillIconCache.find(skillId);
    if (found != m_skillIconCache.end()) {
        return found->second.IsValid() ? &found->second : nullptr;
    }

    shopui::BitmapPixels bitmap;
    const std::string path = ResolveShortcutSkillIconPath(skillId);
    if (!path.empty() && g_fileMgr.IsDataExist(path.c_str())) {
        bitmap = shopui::LoadBitmapPixelsFromGameData(path, true);
    }

    auto inserted = m_skillIconCache.emplace(skillId, std::move(bitmap));
    return inserted.first->second.IsValid() ? &inserted.first->second : nullptr;
}

unsigned long long UIShortCutWnd::BuildDisplayStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_show));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_x));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_y));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_w));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_h));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_hoverSlot + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(g_session.GetShortcutPage()));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_pressedSlot + 1));
    shopui::HashTokenValue(&hash, static_cast<unsigned long long>(m_slotPressArmed));

    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragType));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.source));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.shortcutSlotAbsoluteIndex + 1));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(gameMode->m_dragInfo.itemId));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(gameMode->m_dragInfo.skillId)));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(static_cast<unsigned int>(gameMode->m_dragInfo.skillLevel)));
    }

    for (int slotIndex = 0; slotIndex < kVisibleSlotCount; ++slotIndex) {
        const SHORTCUT_SLOT* slot = g_session.GetShortcutSlotByVisibleIndex(slotIndex);
        const unsigned char isSkill = slot ? slot->isSkill : 0;
        const unsigned int id = slot ? slot->id : 0;
        const unsigned short count = slot ? slot->count : 0;
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(isSkill));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(id));
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(count));

        if (!slot || id == 0) {
            continue;
        }

        if (isSkill != 0) {
            const PLAYER_SKILL_INFO* skill = g_session.GetSkillItemBySkillId(static_cast<int>(id));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(skill ? skill->level : count));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(skill ? skill->spcost : 0));
        } else {
            const ITEM_INFO* item = g_session.GetInventoryItemByItemId(id);
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(item ? item->m_num : 0));
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(item ? item->m_itemIndex : 0));
        }
    }

    return hash;
}
