#include "UIBasicInfoWnd.h"

#include "UIWindowMgr.h"
#include "UIShopCommon.h"
#include "UiTheme.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "render3d/Device.h"
#include "res/ActRes.h"
#include "res/PaletteRes.h"
#include "res/Sprite.h"
#include "session/Session.h"
#include "world/GameActor.h"
#include "world/World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

#if RO_ENABLE_QT6_UI
#include <QImage>
#endif

namespace {

constexpr int kPanelWidth = 288;
constexpr int kWidth = UIBasicInfoWnd::kPanelOffsetX + kPanelWidth;
constexpr int kHeight = 140;
constexpr int kHeaderHeight = 36;
constexpr int kScreenMargin = 10;
// Give the resized layout its own placement so it starts at the new margin.
constexpr char kPlacementKey[] = "BasicInfoWndCompactPortrait";
constexpr int kBarX = 8;
constexpr int kBarWidth = kPanelWidth - 2 * kBarX;
constexpr int kBarHeight = 22;
constexpr int kBarLabelWidth = 40;
constexpr int kPortraitInset = 3;
constexpr int kPortraitImageSize = UIBasicInfoWnd::kPortraitDiameter - 2 * kPortraitInset;
constexpr int kHeadgearMask = 1 | 256 | 512 | 0x0400 | 0x0800 | 0x1000;

struct PortraitLayer {
    CSprRes* sprite = nullptr;
    const CMotion* motion = nullptr;
    POINT offset{};
    unsigned int* palette = nullptr;
    bool affectsFraming = true;
};

const ITEM_INFO* GetPortraitHeadgear(int normalMask, int costumeMask)
{
    const ITEM_INFO* normal = nullptr;
    for (const ITEM_INFO& item : g_session.GetInventoryItems()) {
        // Select the costume separately for each slot, even if other slots
        // contain ordinary headgear or the equipment window hides costumes.
        if ((item.m_wearLocation & costumeMask) != 0) {
            return &item;
        }
        if (!normal && (item.m_wearLocation & normalMask) != 0) {
            normal = &item;
        }
    }
    return normal;
}

POINT GetPortraitAttachmentOffset(const CMotion* head, const CMotion* accessory)
{
    if (!head->attachInfo.empty() && !accessory->attachInfo.empty()) {
        const CAttachPointInfo& base = head->attachInfo.front();
        const CAttachPointInfo& attached = accessory->attachInfo.front();
        if (base.attr == attached.attr) {
            return POINT{ base.x - attached.x, base.y - attached.y };
        }
    }
    return POINT{};
}

bool BuildPlayerPortrait(shopui::BitmapPixels* out)
{
    char path[512]{};
    const int sex = g_session.GetSex();
    const int job = g_session.m_playerJob;
    int head = g_session.m_playerHead;
    const std::string headAct = g_session.GetHeadActName(job, &head, sex, path);
    const std::string headSpr = g_session.GetHeadSprName(job, &head, sex, path);
    CActRes* headAction = g_resMgr.GetAs<CActRes>(headAct.c_str());
    CSprRes* headSprite = g_resMgr.GetAs<CSprRes>(headSpr.c_str());
    const CMotion* headMotion = headAction ? headAction->GetMotion(0, 0) : nullptr;
    if (!headSprite || !headMotion) {
        return false;
    }

    std::array<unsigned int, 256> hairPalette{};
    unsigned int* palette = headSprite->m_pal;
    if (g_session.m_playerHeadPalette > 0) {
        g_session.GetHeadPaletteName(head, job, sex, g_session.m_playerHeadPalette, path);
        if (CPaletteRes* resource = g_resMgr.GetAs<CPaletteRes>(path)) {
            g_3dDevice.ConvertPalette(hairPalette.data(), resource->m_pal, 256);
            palette = hairPalette.data();
        }
    }
    std::vector<PortraitLayer> layers{ { headSprite, headMotion, POINT{}, palette } };
    const std::array<int, 3> normalMasks{ 1, 512, 256 }; // lower, middle, upper
    const std::array<int, 3> costumeMasks{ 0x1000, 0x0800, 0x0400 };
    const std::array<int, 3> visibleViews{
        g_session.m_playerAccessory, g_session.m_playerAccessory3, g_session.m_playerAccessory2
    };
    std::map<std::string, size_t> drawnAccessories;
    for (size_t slot = 0; slot < normalMasks.size(); ++slot) {
        std::string act;
        std::string spr;
        const ITEM_INFO* item = GetPortraitHeadgear(normalMasks[slot], costumeMasks[slot]);
        const std::string resource = item ? g_ttemmgr.GetResourceName(item->GetItemId(), true) : std::string();
        if (!resource.empty()) {
            // Resolve directly from the equipped item, avoiding ambiguous
            // reverse lookups from item artwork to a server view ID.
            act = g_session.GetAccessoryActName(sex, resource, path);
            spr = g_session.GetAccessorySprName(sex, resource, path);
        } else {
            act = g_session.GetAccessoryActName(job, &head, sex, visibleViews[slot], path);
            spr = g_session.GetAccessorySprName(job, &head, sex, visibleViews[slot], path);
        }
        if (act.empty() || spr.empty()) {
            continue;
        }
        const auto drawn = drawnAccessories.find(spr);
        if (drawn != drawnAccessories.end()) {
            // Shared artwork still frames the portrait if it occupies an
            // upper or middle slot as well as the lower slot.
            if (slot != 0) {
                layers[drawn->second].affectsFraming = true;
            }
            continue;
        }
        CActRes* action = g_resMgr.GetAs<CActRes>(act.c_str());
        CSprRes* sprite = g_resMgr.GetAs<CSprRes>(spr.c_str());
        const CMotion* motion = action ? action->GetMotion(0, 0) : nullptr;
        if (sprite && motion) {
            drawnAccessories.emplace(spr, layers.size());
            layers.push_back({ sprite, motion, GetPortraitAttachmentOffset(headMotion, motion), sprite->m_pal, slot != 0 });
        }
    }

    RECT bounds{};
    bool hasBounds = false;
    for (const PortraitLayer& layer : layers) {
        RECT layerBounds{};
        if (!ComputeMotionClipBounds(layer.sprite, layer.motion, &layerBounds)) {
            continue;
        }
        OffsetRect(&layerBounds, layer.offset.x, layer.offset.y);
        if (!hasBounds) {
            bounds = layerBounds;
            hasBounds = true;
        } else {
            bounds.left = (std::min)(bounds.left, layerBounds.left);
            bounds.top = (std::min)(bounds.top, layerBounds.top);
            bounds.right = (std::max)(bounds.right, layerBounds.right);
            bounds.bottom = (std::max)(bounds.bottom, layerBounds.bottom);
        }
    }
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    if (!hasBounds || width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        return false;
    }
    std::vector<unsigned int> composed(static_cast<size_t>(width) * height, 0u);
    std::vector<unsigned int> framingPixels(composed.size(), 0u);
    for (const PortraitLayer& layer : layers) {
        DrawActMotionToArgb(composed.data(), width, height,
            layer.offset.x - bounds.left, layer.offset.y - bounds.top,
            layer.sprite, layer.motion, layer.palette);
        if (layer.affectsFraming) {
            DrawActMotionToArgb(framingPixels.data(), width, height,
                layer.offset.x - bounds.left, layer.offset.y - bounds.top,
                layer.sprite, layer.motion, layer.palette);
        }
    }

    // Fit the head and upper/middle gear. Lower gear remains visible without
    // changing the face's scale or position, including costume lower gear.
    RECT opaque{ width, height, 0, 0 };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if ((framingPixels[static_cast<size_t>(y) * width + x] >> 24) != 0) {
                opaque.left = (std::min)(opaque.left, static_cast<LONG>(x));
                opaque.top = (std::min)(opaque.top, static_cast<LONG>(y));
                opaque.right = (std::max)(opaque.right, static_cast<LONG>(x + 1));
                opaque.bottom = (std::max)(opaque.bottom, static_cast<LONG>(y + 1));
            }
        }
    }
    const int faceWidth = opaque.right - opaque.left;
    const int faceHeight = opaque.bottom - opaque.top;
    if (faceWidth <= 0 || faceHeight <= 0) {
        return false;
    }
    const double scale = (std::min)(1.8, (kPortraitImageSize - 4.0)
        / std::hypot(static_cast<double>(faceWidth), static_cast<double>(faceHeight)));
    const double center = kPortraitImageSize / 2.0;
    out->width = kPortraitImageSize;
    out->height = kPortraitImageSize;
    out->pixels.assign(static_cast<size_t>(kPortraitImageSize) * kPortraitImageSize, 0u);
    for (int y = 0; y < kPortraitImageSize; ++y) {
        for (int x = 0; x < kPortraitImageSize; ++x) {
            const double dx = x + 0.5 - center;
            const double dy = y + 0.5 - center;
            if (dx * dx + dy * dy > center * center) {
                continue;
            }
            const int srcX = static_cast<int>(std::floor(opaque.left + faceWidth / 2.0 + dx / scale));
            const int srcY = static_cast<int>(std::floor(opaque.top + faceHeight / 2.0 + dy / scale));
            if (srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
                out->pixels[static_cast<size_t>(y) * kPortraitImageSize + x] = composed[static_cast<size_t>(srcY) * width + srcX];
            }
        }
    }
    return true;
}

std::string FormatNumber(u32 value)
{
    std::string text = std::to_string(value);
    for (size_t separator = text.size(); separator > 3; separator -= 3) {
        text.insert(separator - 3, ",");
    }
    return text;
}

std::string FormatWeight(u32 value)
{
    std::string text = FormatNumber(value / 10);
    if (value % 10 != 0) {
        text += "." + std::to_string(value % 10);
    }
    return text;
}

HFONT GetBasicInfoFont(int pixelSize)
{
#if RO_PLATFORM_WINDOWS
    static HFONT titleFont = CreateFontA(-16, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    static HFONT barFont = CreateFontA(-14, 0, 0, 0, FW_BOLD,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    static HFONT footerFont = CreateFontA(-12, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    const HFONT font = pixelSize == 16 ? titleFont : (pixelSize == 12 ? footerFont : barFont);
    return font ? font : shopui::GetUiFont();
#else
    (void)pixelSize;
    return shopui::GetUiFont();
#endif
}

void DrawBasicInfoText(HDC hdc, RECT rect, const std::string& text, COLORREF color, UINT flags, int pixelSize = 14)
{
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldTextColor = SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, GetBasicInfoFont(pixelSize));
    const UINT format = flags | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX;
#if RO_ENABLE_QT6_UI
    shopui::DrawWindowTextRectQt(hdc, rect, text, color, format);
#else
    DrawTextA(hdc, text.c_str(), -1, &rect, format);
#endif
    SelectObject(hdc, oldFont);
    SetTextColor(hdc, oldTextColor);
    SetBkMode(hdc, oldBkMode);
}

void DrawBasicInfoBar(HDC hdc, int x, int y, const char* label, int current, int maximum, COLORREF color)
{
    const RECT track{ x, y, x + kBarWidth, y + kBarHeight };
    uitheme::FillRoundedRect(hdc, track, uitheme::kShadow);

    const int fillLeft = x + kBarLabelWidth + 2;
    const int availableWidth = kBarWidth - kBarLabelWidth - 4;
    const int total = (std::max)(1, maximum);
    const int amount = std::clamp(current, 0, total);
    const int fillWidth = static_cast<int>(static_cast<long long>(availableWidth) * amount / total);
    if (fillWidth > 0) {
        const RECT fill{ fillLeft, y + 2, fillLeft + fillWidth, y + kBarHeight - 2 };
        uitheme::FillRoundedRect(hdc, fill, color);
    }
    uitheme::FrameRoundedRect(hdc, track, uitheme::kFrame);

    const RECT labelRect{ x + 2, y, x + kBarLabelWidth, y + kBarHeight };
    DrawBasicInfoText(hdc, labelRect, label, uitheme::kUiText, DT_CENTER);

    const std::string value = std::to_string(current) + " / " + std::to_string(maximum);
    const RECT valueRect{ fillLeft, y, x + kBarWidth - 2, y + kBarHeight };
    // Keep the numbers readable over both the bright fill and the empty track.
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            RECT outline = valueRect;
            OffsetRect(&outline, dx, dy);
            DrawBasicInfoText(hdc, outline, value, uitheme::kShadow, DT_CENTER);
        }
    }
    DrawBasicInfoText(hdc, valueRect, value, RGB(255, 255, 255), DT_CENTER);
}

} // namespace

UIBasicInfoWnd::UIBasicInfoWnd()
{
    Create(kWidth, kHeight);
    int savedX = kScreenMargin;
    int savedY = kScreenMargin;
    if (LoadUiWindowPlacement(kPlacementKey, &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
    }
    Move(savedX, savedY);
}

bool UIBasicInfoWnd::IsUpdateNeed()
{
    return m_show != 0 && (m_isDirty != 0 || !m_hasDrawStateToken
        || BuildDisplayStateToken() != m_lastDrawStateToken);
}

void UIBasicInfoWnd::OnDraw()
{
    if (!IsQtUiRuntimeEnabled()) {
        if (!g_hMainWnd || m_show == 0) {
            return;
        }
        HDC hdc = AcquireDrawTarget();
        if (!hdc) {
            return;
        }

        const int panelX = m_x + kPanelOffsetX;
        const DisplayData data = BuildDisplayData();
        const RECT panel{ panelX, m_y, panelX + kPanelWidth, m_y + m_h };
        uitheme::FillRoundedRect(hdc, panel, uitheme::kPanelBackground);
        uitheme::FrameRoundedRect(hdc, panel, uitheme::kFrame);
        const RECT innerFrame{ panelX + 3, m_y + 3, panelX + kPanelWidth - 3, m_y + m_h - 3 };
        uitheme::FrameRoundedRect(hdc, innerFrame, uitheme::kFrame, (std::max)(0, uitheme::kCornerRadius - 2));

        const RECT nameRect{ panelX + 18, m_y + 6, panelX + kPanelWidth - 100, m_y + 32 };
        DrawBasicInfoText(hdc, nameRect, data.name, uitheme::kAccent, DT_LEFT | DT_END_ELLIPSIS, 16);
        const RECT levelRect{ panelX + kPanelWidth - 90, m_y + 6, panelX + kPanelWidth - 18, m_y + 32 };
        DrawBasicInfoText(hdc, levelRect, "Lv. " + std::to_string(data.level), uitheme::kAccent, DT_RIGHT);
        const RECT divider{ panelX + 8, m_y + 35, panelX + kPanelWidth - 8, m_y + 36 };
        uitheme::FillRoundedRect(hdc, divider, uitheme::kFrame, 0);

        DrawBasicInfoBar(hdc, panelX + kBarX, m_y + 40, "HP", data.hp, data.maxHp, uitheme::kHpBar);
        DrawBasicInfoBar(hdc, panelX + kBarX, m_y + 66, "SP", data.sp, data.maxSp, uitheme::kSpBar);
        const RECT footerDivider{ panelX + 8, m_y + 94, panelX + kPanelWidth - 8, m_y + 95 };
        uitheme::FillRoundedRect(hdc, footerDivider, uitheme::kFrame, 0);
        const RECT weightLabelRect{ panelX + 18, m_y + 98, panelX + 88, m_y + 114 };
        const RECT weightValueRect{ panelX + 92, m_y + 98, panelX + kPanelWidth - 18, m_y + 114 };
        const RECT zenyLabelRect{ panelX + 18, m_y + 117, panelX + 88, m_y + 133 };
        const RECT zenyValueRect{ panelX + 92, m_y + 117, panelX + kPanelWidth - 18, m_y + 133 };
        DrawBasicInfoText(hdc, weightLabelRect, "Weight", uitheme::kUiText, DT_LEFT, 12);
        DrawBasicInfoText(hdc, weightValueRect, data.weightText, uitheme::kUiText, DT_RIGHT | DT_END_ELLIPSIS, 12);
        DrawBasicInfoText(hdc, zenyLabelRect, "Zeny", uitheme::kUiText, DT_LEFT, 12);
        DrawBasicInfoText(hdc, zenyValueRect, data.zenyText, uitheme::kUiText, DT_RIGHT | DT_END_ELLIPSIS, 12);

        const RECT portrait{ m_x, m_y + kPortraitY, m_x + kPortraitDiameter, m_y + kPortraitY + kPortraitDiameter };
        uitheme::FillRoundedRect(hdc, portrait, uitheme::kPanelBackground, kPortraitDiameter / 2);
        for (int inset : { 0, 3 }) {
            const RECT ring{ portrait.left + inset, portrait.top + inset, portrait.right - inset, portrait.bottom - inset };
            uitheme::FrameRoundedRect(hdc, ring, uitheme::kFrame, kPortraitDiameter / 2 - inset);
        }
        const shopui::BitmapPixels& bitmap = GetPortraitPixels();
        if (bitmap.IsValid()) {
            AlphaBlendArgbToHdc(hdc, m_x + kPortraitInset, m_y + kPortraitY + kPortraitInset,
                bitmap.width, bitmap.height, bitmap.pixels.data(), bitmap.width, bitmap.height);
        }
        ReleaseDrawTarget(hdc);
    }

    m_lastDrawStateToken = BuildDisplayStateToken();
    m_hasDrawStateToken = true;
    m_isDirty = 0;
}

void UIBasicInfoWnd::OnLBtnDown(int x, int y)
{
    const int dx = x - (m_x + kPortraitDiameter / 2);
    const int dy = y - (m_y + kPortraitY + kPortraitDiameter / 2);
    const bool portraitHit = dx * dx + dy * dy <= (kPortraitDiameter / 2) * (kPortraitDiameter / 2);
    const bool headerHit = x >= m_x + kPanelOffsetX && x < m_x + m_w
        && y >= m_y && y < m_y + kHeaderHeight;
    if (m_show == 0 || (!portraitHit && !headerHit)) {
        return;
    }
    m_isDragging = 1;
    m_startGlobalX = x;
    m_startGlobalY = y;
    m_orgX = m_x;
    m_orgY = m_y;
}

void UIBasicInfoWnd::StoreInfo()
{
    SaveUiWindowPlacement(kPlacementKey, m_x, m_y);
}

bool UIBasicInfoWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }
    *outData = BuildDisplayData();
    return true;
}

unsigned long long UIBasicInfoWnd::GetPortraitRevision() const
{
    unsigned long long hash = 1469598103934665603ull;
    for (int value : { g_session.GetSex(), g_session.m_playerJob, g_session.m_playerHead,
        g_session.m_playerHeadPalette, g_session.m_playerAccessory,
        g_session.m_playerAccessory3, g_session.m_playerAccessory2 }) {
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(value));
    }
    for (const ITEM_INFO& item : g_session.GetInventoryItems()) {
        if ((item.m_wearLocation & kHeadgearMask) != 0) {
            shopui::HashTokenValue(&hash, item.GetItemId());
            shopui::HashTokenValue(&hash, static_cast<unsigned long long>(item.m_wearLocation & kHeadgearMask));
        }
    }
    return hash;
}

const shopui::BitmapPixels& UIBasicInfoWnd::GetPortraitPixels() const
{
    const unsigned long long revision = GetPortraitRevision();
    if (!m_hasPortraitPixels || revision != m_lastPortraitRevision) {
        m_portraitPixels.Clear();
        m_hasPortraitPixels = BuildPlayerPortrait(&m_portraitPixels);
        m_lastPortraitRevision = revision;
    }
    return m_portraitPixels;
}

bool UIBasicInfoWnd::BuildQtPortraitImage(QImage* outImage) const
{
#if RO_ENABLE_QT6_UI
    if (!outImage) {
        return false;
    }
    const shopui::BitmapPixels& bitmap = GetPortraitPixels();
    if (!bitmap.IsValid()) {
        return false;
    }
    const QImage image(reinterpret_cast<const uchar*>(bitmap.pixels.data()),
        bitmap.width, bitmap.height, bitmap.width * static_cast<int>(sizeof(unsigned int)),
        QImage::Format_ARGB32_Premultiplied);
    *outImage = image.copy();
    return !outImage->isNull();
#else
    (void)outImage;
    return false;
#endif
}

UIBasicInfoWnd::DisplayData UIBasicInfoWnd::BuildDisplayData() const
{
    DisplayData data;
    data.name = g_session.GetPlayerName() ? g_session.GetPlayerName() : "";
    data.weightText = FormatWeight(g_session.m_weight) + " / " + FormatWeight(g_session.m_maxWeight);
    data.zenyText = "0";

    if (const CHARACTER_INFO* info = g_session.GetSelectedCharacterInfo()) {
        if (data.name.empty()) {
            char name[sizeof(info->name) + 1]{};
            std::memcpy(name, info->name, sizeof(info->name));
            data.name = name;
        }
        data.level = info->level;
        data.hp = info->hp;
        data.maxHp = info->maxhp;
        data.sp = info->sp;
        data.maxSp = info->maxsp;
        data.zenyText = FormatNumber(static_cast<u32>((std::max)(0, info->money)));
    }
    if (g_world.m_player) {
        data.hp = static_cast<int>(g_world.m_player->m_Hp);
        data.maxHp = static_cast<int>(g_world.m_player->m_MaxHp);
        data.sp = static_cast<int>(g_world.m_player->m_Sp);
        data.maxSp = static_cast<int>(g_world.m_player->m_MaxSp);
        if (data.level <= 0) {
            data.level = static_cast<int>(g_world.m_player->m_clevel);
        }
    }
    data.level = (std::max)(data.level, 0);
    data.hp = (std::max)(data.hp, 0);
    data.maxHp = (std::max)(data.maxHp, data.hp);
    data.sp = (std::max)(data.sp, 0);
    data.maxSp = (std::max)(data.maxSp, data.sp);
    return data;
}

unsigned long long UIBasicInfoWnd::BuildDisplayStateToken() const
{
    const DisplayData data = BuildDisplayData();
    unsigned long long hash = 1469598103934665603ull;
    for (int value : { m_show, m_x, m_y, m_w, m_h, data.level, data.hp, data.maxHp, data.sp, data.maxSp }) {
        shopui::HashTokenValue(&hash, static_cast<unsigned long long>(value));
    }
    shopui::HashTokenString(&hash, data.name);
    shopui::HashTokenString(&hash, data.weightText);
    shopui::HashTokenString(&hash, data.zenyText);
    shopui::HashTokenValue(&hash, GetPortraitRevision());
    return hash;
}
