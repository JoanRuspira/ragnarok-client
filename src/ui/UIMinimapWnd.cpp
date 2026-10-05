#include "UIMinimapWnd.h"
#include "ui/UiTheme.h"

#include "UIWindowMgr.h"
#include "UiScale.h"
#include "core/File.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"
#include "res/Bitmap.h"
#include "session/Session.h"
#include "world/World.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#endif

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

namespace {

constexpr int kWindowHeight = 240;
constexpr int kTitleBarHeight = 32;
constexpr int kBodyInset = 5;
constexpr int kDefaultScreenMargin = 10;
constexpr int kZoomButtonSize = 22;
constexpr int kZoomButtonGap = 6;
constexpr int kZoomButtonTop = kWindowHeight - kBodyInset - kZoomButtonSize;
constexpr int kMapTop = kTitleBarHeight;
constexpr int kMapHeight = kZoomButtonTop - kMapTop - 5;
constexpr int kMapWidth = kMapHeight;
constexpr int kWindowWidth = kMapWidth + 2 * kBodyInset;
constexpr int kWindowCornerRadius = uitheme::kCornerRadius;
// Start the narrower panel at the corner without inheriting the wider layout's position.
constexpr char kPlacementKey[] = "RoMapWndFitted";
constexpr u32 kDynamicRefreshIntervalMs = 100;
constexpr float kMinZoom = 1.0f;
constexpr float kMaxZoom = 4.0f;
constexpr float kZoomStep = 0.25f;

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') {
            return static_cast<char>(ch - 'A' + 'a');
        }
        return static_cast<char>(ch);
    });
    return value;
}

std::string NormalizeSlash(std::string value)
{
    std::replace(value.begin(), value.end(), '/', '\\');
    return value;
}

void AddUniqueCandidate(std::vector<std::string>& out, const std::string& raw)
{
    if (raw.empty()) {
        return;
    }

    const std::string normalized = NormalizeSlash(raw);
    const std::string lowered = ToLowerAscii(normalized);
    for (const std::string& existing : out) {
        if (ToLowerAscii(existing) == lowered) {
            return;
        }
    }
    out.push_back(normalized);
}

std::vector<std::string> BuildMinimapCandidates(const std::string& bitmapName)
{
    std::vector<std::string> out;
    if (bitmapName.empty()) {
        return out;
    }

    const std::array<const char*, 16> prefixes = {
        "",
        "texture\\",
        "texture\\map\\",
        "texture\\minimap\\",
        "texture\\interface\\map\\",
        "texture\\interface\\minimap\\",
        "data\\",
        "data\\texture\\",
        "data\\texture\\map\\",
        "data\\texture\\minimap\\",
        "data\\texture\\interface\\map\\",
        "data\\texture\\interface\\minimap\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\map\\",
        "texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\minimap\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\map\\",
        "data\\texture\\\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA\\minimap\\"
    };

    std::string normalized = NormalizeSlash(bitmapName);
    AddUniqueCandidate(out, normalized);

    std::string filenameOnly = normalized;
    const size_t slashPos = filenameOnly.find_last_of('\\');
    if (slashPos != std::string::npos && slashPos + 1 < filenameOnly.size()) {
        filenameOnly = filenameOnly.substr(slashPos + 1);
    }

    for (const char* prefix : prefixes) {
        AddUniqueCandidate(out, std::string(prefix) + filenameOnly);
    }

    return out;
}

std::string ResolveMinimapPath(const std::string& bitmapName)
{
    for (const std::string& candidate : BuildMinimapCandidates(bitmapName)) {
        if (g_fileMgr.IsDataExist(candidate.c_str())) {
            return candidate;
        }
    }
    return std::string();
}

float NormalizeAngle360(float angle)
{
    while (angle < 0.0f) {
        angle += 360.0f;
    }
    while (angle >= 360.0f) {
        angle -= 360.0f;
    }
    return angle;
}

int ResolveFacingDirFromRotationDegrees(float rotationDegrees)
{
    return static_cast<int>((NormalizeAngle360(rotationDegrees) + 22.5f) / 45.0f) & 7;
}

int ResolveDisplayedPlayerDir()
{
    if (g_world.m_player && std::isfinite(g_world.m_player->m_roty)) {
        return ResolveFacingDirFromRotationDegrees(g_world.m_player->m_roty);
    }

    // Match the actor-facing convention used by PacketDirToRotationDegrees.
    return ((g_session.m_playerDir & 7) + 4) & 7;
}

void DrawBgraPixelsStretched(HDC target,
    const u32* pixels,
    int bitmapWidth,
    int bitmapHeight,
    const RECT& dst,
    const RECT* srcRect = nullptr)
{
    if (!target || !pixels || bitmapWidth <= 0 || bitmapHeight <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return;
    }
    RECT src{ 0, 0, bitmapWidth, bitmapHeight };
    if (srcRect) {
        src = *srcRect;
    }
    if (src.left < 0 || src.top < 0 || src.right > bitmapWidth || src.bottom > bitmapHeight || src.right <= src.left || src.bottom <= src.top) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = bitmapWidth;
    bmi.bmiHeader.biHeight = -bitmapHeight;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int oldStretchMode = SetStretchBltMode(target, COLORONCOLOR);
    StretchDIBits(target,
        dst.left,
        dst.top,
        dst.right - dst.left,
        dst.bottom - dst.top,
        src.left,
        src.top,
        src.right - src.left,
        src.bottom - src.top,
        pixels,
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY);
    SetStretchBltMode(target, oldStretchMode);
}

RECT FitRectPreservingAspect(const RECT& dst, int srcWidth, int srcHeight)
{
    RECT fitted = dst;
    if (srcWidth <= 0 || srcHeight <= 0 || dst.right <= dst.left || dst.bottom <= dst.top) {
        return fitted;
    }

    const float srcAspect = static_cast<float>(srcWidth) / static_cast<float>(srcHeight);
    const float dstAspect = static_cast<float>(dst.right - dst.left) / static_cast<float>(dst.bottom - dst.top);
    if (srcAspect > dstAspect) {
        const int fittedHeight = static_cast<int>((dst.right - dst.left) / srcAspect);
        const int pad = ((dst.bottom - dst.top) - fittedHeight) / 2;
        fitted.top += pad;
        fitted.bottom = fitted.top + fittedHeight;
    } else {
        const int fittedWidth = static_cast<int>((dst.bottom - dst.top) * srcAspect);
        const int pad = ((dst.right - dst.left) - fittedWidth) / 2;
        fitted.left += pad;
        fitted.right = fitted.left + fittedWidth;
    }
    return fitted;
}

bool BuildMarkerPoint(const RECT& attrArea,
    const RECT& drawRect,
    bool zoomedView,
    const RECT& srcRect,
    int bitmapWidth,
    int bitmapHeight,
    int cellX,
    int cellY,
    POINT* outPoint)
{
    if (!outPoint) {
        return false;
    }

    const int attrWidth = attrArea.right - attrArea.left;
    const int attrHeight = attrArea.bottom - attrArea.top;
    if (attrWidth <= 0 || attrHeight <= 0 || drawRect.right <= drawRect.left || drawRect.bottom <= drawRect.top) {
        return false;
    }

    const float normX = static_cast<float>(cellX - attrArea.left) / static_cast<float>(attrWidth);
    const float normY = 1.0f - (static_cast<float>(cellY - attrArea.top) / static_cast<float>(attrHeight));
    const float clampedNormX = (std::max)(0.0f, (std::min)(1.0f, normX));
    const float clampedNormY = (std::max)(0.0f, (std::min)(1.0f, normY));

    if (!zoomedView || bitmapWidth <= 0 || bitmapHeight <= 0) {
        outPoint->x = drawRect.left + static_cast<int>((drawRect.right - drawRect.left) * clampedNormX);
        outPoint->y = drawRect.top + static_cast<int>((drawRect.bottom - drawRect.top) * clampedNormY);
        return true;
    }

    const float bitmapX = clampedNormX * static_cast<float>(bitmapWidth);
    const float bitmapY = clampedNormY * static_cast<float>(bitmapHeight);
    if (bitmapX < static_cast<float>(srcRect.left) || bitmapX > static_cast<float>(srcRect.right)
        || bitmapY < static_cast<float>(srcRect.top) || bitmapY > static_cast<float>(srcRect.bottom)) {
        return false;
    }

    const int srcWidth = (std::max)(1, static_cast<int>(srcRect.right - srcRect.left));
    const int srcHeight = (std::max)(1, static_cast<int>(srcRect.bottom - srcRect.top));
    const float relX = (bitmapX - static_cast<float>(srcRect.left)) / static_cast<float>(srcWidth);
    const float relY = (bitmapY - static_cast<float>(srcRect.top)) / static_cast<float>(srcHeight);
    outPoint->x = drawRect.left + static_cast<int>((drawRect.right - drawRect.left) * relX);
    outPoint->y = drawRect.top + static_cast<int>((drawRect.bottom - drawRect.top) * relY);
    return true;
}

void FillSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);
}

std::array<RECT, 2> GetZoomSignRects(const RECT& buttonRect)
{
    const LONG centerX = (buttonRect.left + buttonRect.right) / 2;
    const LONG centerY = (buttonRect.top + buttonRect.bottom) / 2;
    const RECT horizontal{ centerX - 5, centerY - 1, centerX + 5, centerY + 1 };
    const RECT vertical{ centerX - 1, centerY - 5, centerX + 1, centerY + 5 };
    return { horizontal, vertical };
}

void FrameSolidRect(HDC hdc, const RECT& rect, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(hdc, &rect, brush);
    DeleteObject(brush);
}

#if RO_ENABLE_QT6_UI
QFont BuildMinimapFontFromHdc(HDC hdc)
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

Qt::Alignment ToQtTextAlignment(UINT format)
{
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop;
    if (format & DT_CENTER) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignHCenter;
    } else if (format & DT_RIGHT) {
        alignment &= ~Qt::AlignLeft;
        alignment |= Qt::AlignRight;
    }

    if (format & DT_VCENTER) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignVCenter;
    } else if (format & DT_BOTTOM) {
        alignment &= ~Qt::AlignTop;
        alignment |= Qt::AlignBottom;
    }

    return alignment;
}

void DrawWindowTextQt(HDC hdc, const RECT& rect, const char* text, COLORREF color, UINT format)
{
    if (!hdc || !text || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const QString label = QString::fromLocal8Bit(text);
    if (label.isEmpty()) {
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
    painter.setFont(BuildMinimapFontFromHdc(hdc));
    painter.setPen(QColor(GetRValue(color), GetGValue(color), GetBValue(color)));
    painter.drawText(QRect(0, 0, width, height), ToQtTextAlignment(format) | Qt::TextSingleLine, label);
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

void DrawMinimapText(HDC hdc, RECT rect, const char* text, COLORREF color,
    UINT format = DT_LEFT | DT_VCENTER)
{
#if RO_PLATFORM_WINDOWS
    static HFONT titleFont = CreateFontA(-14, 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, uitheme::kUiFontFamily);
    HFONT font = titleFont;
#else
    HFONT font = shopui::GetUiFont();
#endif
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    const COLORREF oldColor = SetTextColor(hdc, color);
    HGDIOBJ oldFont = SelectObject(hdc, font ? font : shopui::GetUiFont());
    format |= DT_SINGLELINE | DT_NOPREFIX;
#if RO_ENABLE_QT6_UI
    DrawWindowTextQt(hdc, rect, text ? text : "", color, format);
#else
    DrawTextA(hdc, text ? text : "", -1, &rect, format);
#endif
    SelectObject(hdc, oldFont);
    SetTextColor(hdc, oldColor);
    SetBkMode(hdc, oldBkMode);
}

void DrawSmallMarker(HDC hdc, const POINT& pt, COLORREF fillColor, COLORREF outlineColor, int radius)
{
    HBRUSH brush = CreateSolidBrush(fillColor);
    HPEN pen = CreatePen(PS_SOLID, 1, outlineColor);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    Ellipse(hdc, pt.x - radius, pt.y - radius, pt.x + radius + 1, pt.y + radius + 1);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
}

std::string StripExtension(const std::string& value)
{
    std::string out = value;
    const size_t slashPos = out.find_last_of("\\/");
    if (slashPos != std::string::npos && slashPos + 1 < out.size()) {
        out = out.substr(slashPos + 1);
    }
    const size_t dotPos = out.find_last_of('.');
    if (dotPos != std::string::npos) {
        out.resize(dotPos);
    }
    return out;
}

bool BlitArgbCacheToHdc(HDC target, int x, int y, int width, int height, const void* bits)
{
    if (!target || !bits || width <= 0 || height <= 0) {
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    return StretchDIBits(
               target,
               x,
               y,
               width,
               height,
               0,
               0,
               width,
               height,
               bits,
               &bmi,
               DIB_RGB_COLORS,
               SRCCOPY) != GDI_ERROR;
}

} // namespace

UIRoMapWnd::UIRoMapWnd()
    : m_controlsCreated(false),
      m_renderCacheSurface(),
      m_renderCacheDirty(true),
      m_mapBitmapWidth(0),
      m_mapBitmapHeight(0),
      m_lastVisualStateToken(0ull),
      m_hasVisualStateToken(false),
      m_lastPlayerX(std::numeric_limits<int>::min()),
      m_lastPlayerY(std::numeric_limits<int>::min()),
      m_lastPlayerDir(std::numeric_limits<int>::min()),
      m_lastDynamicInvalidateTick(0),
      m_pressedZoomDirection(0)
{
    Create(kWindowWidth, kWindowHeight);

    RECT clientRect{ 0, 0, 640, 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    const int clientWidth = IsQtUiRuntimeEnabled()
        ? UiScaleRawToLogicalCoordinate(clientRect.right - clientRect.left)
        : static_cast<int>(clientRect.right - clientRect.left);
    int defaultX = clientRect.left + clientWidth - kWindowWidth - kDefaultScreenMargin;
    int defaultY = clientRect.top + kDefaultScreenMargin;
    g_windowMgr.ClampWindowToClient(&defaultX, &defaultY, m_w, m_h);
    Move(defaultX, defaultY);

    int savedX = m_x;
    int savedY = m_y;
    if (LoadUiWindowPlacement(kPlacementKey, &savedX, &savedY)) {
        g_windowMgr.ClampWindowToClient(&savedX, &savedY, m_w, m_h);
        Move(savedX, savedY);
    }
}

UIRoMapWnd::~UIRoMapWnd()
{
    ReleaseAssets();
}

void UIRoMapWnd::SetShow(int show)
{
    if (show == 0) {
        m_pressedZoomDirection = 0;
        UIWindow::SetShow(0);
        return;
    }

    EnsureCreated();
    UpdateMinimapBitmap();
    if (m_mapPixels.empty() || m_mapBitmapWidth <= 0 || m_mapBitmapHeight <= 0) {
        UIWindow::SetShow(0);
        return;
    }

    UIWindow::SetShow(1);
    InvalidateRenderCache();
}

bool UIRoMapWnd::IsUpdateNeed()
{
    if (m_show == 0) {
        return false;
    }
    if (m_isDirty != 0 || !m_hasVisualStateToken) {
        return true;
    }
    return BuildVisualStateToken() != m_lastVisualStateToken;
}

void UIRoMapWnd::OnCreate(int x, int y)
{
    (void)x;
    (void)y;
    if (m_controlsCreated) {
        return;
    }
    m_controlsCreated = true;
    UpdateMinimapBitmap();
}

void UIRoMapWnd::OnLBtnDown(int x, int y)
{
    if (m_show == 0) {
        return;
    }
    for (int direction : { -1, 1 }) {
        const RECT rect = GetZoomButtonRect(direction);
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            if (CanZoom(direction)) {
                m_pressedZoomDirection = direction;
                InvalidateRenderCache();
                Invalidate();
            }
            return;
        }
    }
    if (x < m_x || x >= m_x + m_w || y < m_y || y >= m_y + kTitleBarHeight) {
        return;
    }
    m_isDragging = 1;
    m_startGlobalX = x;
    m_startGlobalY = y;
    m_orgX = m_x;
    m_orgY = m_y;
}

void UIRoMapWnd::OnLBtnDblClk(int x, int y)
{
    OnLBtnDown(x, y);
}

void UIRoMapWnd::OnLBtnUp(int x, int y)
{
    if (m_pressedZoomDirection != 0) {
        const int direction = m_pressedZoomDirection;
        const RECT rect = GetZoomButtonRect(direction);
        m_pressedZoomDirection = 0;
        if (x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom) {
            OnWheel(direction * 120);
        }
        InvalidateRenderCache();
        Invalidate();
        return;
    }
    UIFrameWnd::OnLBtnUp(x, y);
}

void UIRoMapWnd::OnProcess()
{
    UIFrameWnd::OnProcess();
    if (m_show == 0) {
        return;
    }

    const int playerX = g_session.m_playerPosX;
    const int playerY = g_session.m_playerPosY;
    const int playerDir = ResolveDisplayedPlayerDir();
    if (playerX == m_lastPlayerX && playerY == m_lastPlayerY && playerDir == m_lastPlayerDir) {
        return;
    }

    const u32 now = GetTickCount();
    if (m_lastDynamicInvalidateTick != 0 && now - m_lastDynamicInvalidateTick < kDynamicRefreshIntervalMs) {
        return;
    }

    m_lastPlayerX = playerX;
    m_lastPlayerY = playerY;
    m_lastPlayerDir = playerDir;
    m_lastDynamicInvalidateTick = now;
    InvalidateRenderCache();
    Invalidate();
}

void UIRoMapWnd::DrawWindowContents(HDC hdc, int baseX, int baseY)
{
    if (!hdc) {
        return;
    }

    RECT windowRect{ baseX, baseY, baseX + m_w, baseY + m_h };
    RECT mapRect{ baseX + kBodyInset, baseY + kMapTop, baseX + kBodyInset + kMapWidth, baseY + kMapTop + kMapHeight };
    const int savedDc = SaveDC(hdc);
    HRGN clipRgn = CreateRoundRectRgn(
        windowRect.left,
        windowRect.top,
        windowRect.right + 1,
        windowRect.bottom + 1,
        kWindowCornerRadius * 2,
        kWindowCornerRadius * 2);
    if (clipRgn) {
        SelectClipRgn(hdc, clipRgn);
    }

    FillSolidRect(hdc, windowRect, uitheme::kPanelBackground);
    FillSolidRect(hdc, RECT{ baseX + kBodyInset, baseY + 28, baseX + m_w - kBodyInset, baseY + 29 }, uitheme::kFrame);
    FillSolidRect(hdc, mapRect, uitheme::kShadow);

    RECT drawRect = mapRect;
    RECT srcRect{ 0, 0, m_mapBitmapWidth, m_mapBitmapHeight };
    bool zoomedView = false;

    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0) {
        if (zoom > kMinZoom + 0.001f) {
            zoomedView = true;
            const float viewAspect = static_cast<float>(mapRect.right - mapRect.left) / static_cast<float>(mapRect.bottom - mapRect.top);
            const float bitmapAspect = static_cast<float>(m_mapBitmapWidth) / static_cast<float>(m_mapBitmapHeight);

            int srcWidth = 0;
            int srcHeight = 0;
            if (bitmapAspect > viewAspect) {
                srcHeight = static_cast<int>(m_mapBitmapHeight / zoom);
                srcWidth = static_cast<int>(srcHeight * viewAspect);
            } else {
                srcWidth = static_cast<int>(m_mapBitmapWidth / zoom);
                srcHeight = static_cast<int>(srcWidth / viewAspect);
            }

            srcWidth = (std::max)(1, (std::min)(srcWidth, m_mapBitmapWidth));
            srcHeight = (std::max)(1, (std::min)(srcHeight, m_mapBitmapHeight));

            float centerNormX = 0.5f;
            float centerNormY = 0.5f;
            const int attrWidth = attrArea.right - attrArea.left;
            const int attrHeight = attrArea.bottom - attrArea.top;
            if (attrWidth > 0 && attrHeight > 0) {
                centerNormX = static_cast<float>(g_session.m_playerPosX - attrArea.left) / static_cast<float>(attrWidth);
                centerNormY = 1.0f - (static_cast<float>(g_session.m_playerPosY - attrArea.top) / static_cast<float>(attrHeight));
                centerNormX = (std::max)(0.0f, (std::min)(1.0f, centerNormX));
                centerNormY = (std::max)(0.0f, (std::min)(1.0f, centerNormY));
            }

            const int centerX = static_cast<int>(centerNormX * m_mapBitmapWidth);
            const int centerY = static_cast<int>(centerNormY * m_mapBitmapHeight);
            srcRect.left = centerX - srcWidth / 2;
            srcRect.top = centerY - srcHeight / 2;
            srcRect.left = (std::max)(0, (std::min)(static_cast<int>(srcRect.left), m_mapBitmapWidth - srcWidth));
            srcRect.top = (std::max)(0, (std::min)(static_cast<int>(srcRect.top), m_mapBitmapHeight - srcHeight));
            srcRect.right = srcRect.left + srcWidth;
            srcRect.bottom = srcRect.top + srcHeight;
            DrawBgraPixelsStretched(hdc, m_mapPixels.data(), m_mapBitmapWidth, m_mapBitmapHeight, mapRect, &srcRect);
        } else {
            drawRect = FitRectPreservingAspect(mapRect, m_mapBitmapWidth, m_mapBitmapHeight);
            DrawBgraPixelsStretched(hdc, m_mapPixels.data(), m_mapBitmapWidth, m_mapBitmapHeight, drawRect);
        }
    } else {
        FillSolidRect(hdc, mapRect, RGB(62, 88, 52));
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(118, 150, 96));
        HGDIOBJ oldPen = SelectObject(hdc, gridPen);
        for (int i = 1; i < 12; ++i) {
            const int xLine = mapRect.left + ((mapRect.right - mapRect.left) * i) / 12;
            MoveToEx(hdc, xLine, mapRect.top, nullptr);
            LineTo(hdc, xLine, mapRect.bottom);
        }
        for (int i = 1; i < 8; ++i) {
            const int yLine = mapRect.top + ((mapRect.bottom - mapRect.top) * i) / 8;
            MoveToEx(hdc, mapRect.left, yLine, nullptr);
            LineTo(hdc, mapRect.right, yLine);
        }
        SelectObject(hdc, oldPen);
        DeleteObject(gridPen);
    }

    const std::string mapName = StripExtension(GetCurrentMinimapBitmapName());
    const std::string title = mapName.empty() ? "Map" : mapName;
    DrawMinimapText(hdc, RECT{ baseX + kBodyInset, baseY + 4, baseX + m_w - kBodyInset, baseY + 28 },
        title.c_str(), uitheme::kUiText, DT_CENTER | DT_VCENTER | DT_END_ELLIPSIS);

    const int mapSavedDc = SaveDC(hdc);
    IntersectClipRect(hdc, mapRect.left + 1, mapRect.top + 1, mapRect.right - 1, mapRect.bottom - 1);
    auto drawWorldPoint = [&](int cellX, int cellY, COLORREF fillColor, COLORREF outlineColor, int radius) {
        POINT pt{};
        if (BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight, cellX, cellY, &pt)) {
            DrawSmallMarker(hdc, pt, fillColor, outlineColor, radius);
        }
    };

    if (gameMode) {
        for (const auto& entry : gameMode->m_partyPosList) {
            drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 2);
        }
        for (const auto& entry : gameMode->m_guildPosList) {
            drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 2);
        }
        if (g_windowMgr.m_isDrawCompass != 0) {
            for (const auto& entry : gameMode->m_compassPosList) {
                drawWorldPoint(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, RGB(24, 24, 24), 3);
            }
        }
    }

    POINT playerPoint{};
    if (BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight,
            g_session.m_playerPosX, g_session.m_playerPosY, &playerPoint)) {
        DrawSmallMarker(hdc, playerPoint, RGB(245, 224, 126), RGB(0, 0, 0), 4);
    }

    RestoreDC(hdc, mapSavedDc);
    FrameSolidRect(hdc, mapRect, uitheme::kFrame);
    DrawZoomButtons(hdc, baseX, baseY);

    RestoreDC(hdc, savedDc);
    if (clipRgn) {
        DeleteObject(clipRgn);
    }
    uitheme::FrameRoundedRect(hdc, windowRect, uitheme::kFrame);
    const RECT innerFrame{ windowRect.left + 3, windowRect.top + 3, windowRect.right - 3, windowRect.bottom - 3 };
    uitheme::FrameRoundedRect(hdc, innerFrame, uitheme::kFrame, (std::max)(0, kWindowCornerRadius - 2));
}

void UIRoMapWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    if (IsQtUiRuntimeEnabled()) {
        UpdateMinimapBitmap();
        m_lastVisualStateToken = BuildVisualStateToken();
        m_hasVisualStateToken = true;
        m_isDirty = 0;
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    DrawToHdc(hdc, m_x, m_y);
    ReleaseDrawTarget(hdc);
}

void UIRoMapWnd::DrawToHdc(HDC hdc, int drawX, int drawY)
{
    if (!hdc || m_show == 0) {
        return;
    }

    EnsureCreated();
    UpdateMinimapBitmap();

    const unsigned long long visualStateToken = BuildVisualStateToken();
    if (!m_hasVisualStateToken || visualStateToken != m_lastVisualStateToken) {
        InvalidateRenderCache();
    }

    EnsureRenderCache();
    if (m_renderCacheSurface.IsValid()) {
        if (m_renderCacheDirty) {
            DrawWindowContents(m_renderCacheSurface.GetDC(), 0, 0);
            m_renderCacheDirty = false;
        }
        if (!BlitArgbCacheToHdc(hdc, drawX, drawY, m_w, m_h, m_renderCacheSurface.GetBits())) {
            DrawWindowContents(hdc, drawX, drawY);
        }
    } else {
        DrawWindowContents(hdc, drawX, drawY);
    }

    m_lastVisualStateToken = visualStateToken;
    m_hasVisualStateToken = true;
    m_isDirty = 0;
}

void UIRoMapWnd::OnWheel(int delta)
{
    if (delta == 0) {
        return;
    }
    const float direction = delta > 0 ? 1.0f : -1.0f;
    const float nextZoom = g_windowMgr.m_miniMapZoomFactor + direction * kZoomStep;
    g_windowMgr.m_miniMapZoomFactor = (std::max)(kMinZoom, (std::min)(kMaxZoom, nextZoom));
    InvalidateRenderCache();
    Invalidate();
}

void UIRoMapWnd::StoreInfo()
{
    SaveUiWindowPlacement(kPlacementKey, m_x, m_y);
}

bool UIRoMapWnd::GetDisplayDataForQt(DisplayData* outData) const
{
    if (!outData) {
        return false;
    }

    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();

    DisplayData data{};
    data.imageRevision = static_cast<int>(BuildVisualStateToken() & 0x7fffffff);

    const RECT mapRect{ m_x + kBodyInset, m_y + kMapTop, m_x + kBodyInset + kMapWidth, m_y + kMapTop + kMapHeight };
    data.mapX = mapRect.left;
    data.mapY = mapRect.top;
    data.mapWidth = mapRect.right - mapRect.left;
    data.mapHeight = mapRect.bottom - mapRect.top;

    for (size_t index = 0; index < data.zoomButtons.size(); ++index) {
        DisplayZoomButton& button = data.zoomButtons[index];
        button.direction = index == 0 ? -1 : 1;
        const RECT rect = GetZoomButtonRect(button.direction);
        button.x = rect.left;
        button.y = rect.top;
        button.width = rect.right - rect.left;
        button.height = rect.bottom - rect.top;
        button.enabled = CanZoom(button.direction);
        button.pressed = m_pressedZoomDirection == button.direction;
    }

    data.mapName = StripExtension(GetCurrentMinimapBitmapName());

    RECT drawRect = mapRect;
    RECT srcRect{ 0, 0, m_mapBitmapWidth, m_mapBitmapHeight };
    bool zoomedView = false;

    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0) {
        if (zoom > kMinZoom + 0.001f) {
            zoomedView = true;
            const float viewAspect = static_cast<float>(mapRect.right - mapRect.left) / static_cast<float>(mapRect.bottom - mapRect.top);
            const float bitmapAspect = static_cast<float>(m_mapBitmapWidth) / static_cast<float>(m_mapBitmapHeight);

            int srcWidth = 0;
            int srcHeight = 0;
            if (bitmapAspect > viewAspect) {
                srcHeight = static_cast<int>(m_mapBitmapHeight / zoom);
                srcWidth = static_cast<int>(srcHeight * viewAspect);
            } else {
                srcWidth = static_cast<int>(m_mapBitmapWidth / zoom);
                srcHeight = static_cast<int>(srcWidth / viewAspect);
            }

            srcWidth = (std::max)(1, (std::min)(srcWidth, m_mapBitmapWidth));
            srcHeight = (std::max)(1, (std::min)(srcHeight, m_mapBitmapHeight));

            float centerNormX = 0.5f;
            float centerNormY = 0.5f;
            const int attrWidth = attrArea.right - attrArea.left;
            const int attrHeight = attrArea.bottom - attrArea.top;
            if (attrWidth > 0 && attrHeight > 0) {
                centerNormX = static_cast<float>(g_session.m_playerPosX - attrArea.left) / static_cast<float>(attrWidth);
                centerNormY = 1.0f - (static_cast<float>(g_session.m_playerPosY - attrArea.top) / static_cast<float>(attrHeight));
                centerNormX = (std::max)(0.0f, (std::min)(1.0f, centerNormX));
                centerNormY = (std::max)(0.0f, (std::min)(1.0f, centerNormY));
            }

            const int centerX = static_cast<int>(centerNormX * m_mapBitmapWidth);
            const int centerY = static_cast<int>(centerNormY * m_mapBitmapHeight);
            srcRect.left = centerX - srcWidth / 2;
            srcRect.top = centerY - srcHeight / 2;
            srcRect.left = (std::max)(0, (std::min)(static_cast<int>(srcRect.left), m_mapBitmapWidth - srcWidth));
            srcRect.top = (std::max)(0, (std::min)(static_cast<int>(srcRect.top), m_mapBitmapHeight - srcHeight));
            srcRect.right = srcRect.left + srcWidth;
            srcRect.bottom = srcRect.top + srcHeight;
        } else {
            drawRect = FitRectPreservingAspect(mapRect, m_mapBitmapWidth, m_mapBitmapHeight);
        }
    }

    auto appendMarker = [&](int cellX, int cellY, COLORREF fillColor, int radius) {
        POINT pt{};
        if (!BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight, cellX, cellY, &pt)) {
            return;
        }
        DisplayMarker marker{};
        marker.x = pt.x;
        marker.y = pt.y;
        marker.radius = radius;
        marker.color = static_cast<unsigned int>(fillColor & 0x00FFFFFFu);
        data.markers.push_back(marker);
    };

    if (gameMode) {
        for (const auto& entry : gameMode->m_partyPosList) {
            appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 2);
        }
        for (const auto& entry : gameMode->m_guildPosList) {
            appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 2);
        }
        if (g_windowMgr.m_isDrawCompass != 0) {
            for (const auto& entry : gameMode->m_compassPosList) {
                appendMarker(entry.second.x, entry.second.y, entry.second.color & 0x00FFFFFFu, 3);
            }
        }
    }

    POINT playerPoint{};
    if (BuildMarkerPoint(attrArea, drawRect, zoomedView, srcRect, m_mapBitmapWidth, m_mapBitmapHeight,
            g_session.m_playerPosX, g_session.m_playerPosY, &playerPoint)) {
        data.playerVisible = true;
        data.playerX = playerPoint.x;
        data.playerY = playerPoint.y;
        data.playerDirection = ResolveDisplayedPlayerDir();
    }

    *outData = std::move(data);
    return true;
}

bool UIRoMapWnd::HasLoadedMinimap() const
{
    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();
    return !m_mapPixels.empty() && m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0;
}

bool UIRoMapWnd::BuildQtMinimapImage(QImage* outImage) const
{
#if !RO_ENABLE_QT6_UI
    (void)outImage;
    return false;
#else
    if (!outImage) {
        return false;
    }

    UIRoMapWnd* mutableThis = const_cast<UIRoMapWnd*>(this);
    mutableThis->EnsureCreated();
    mutableThis->UpdateMinimapBitmap();

    if (m_mapBitmapWidth <= 0 || m_mapBitmapHeight <= 0 || m_mapPixels.empty()) {
        return false;
    }

    const QImage sourceImage(
        reinterpret_cast<const uchar*>(m_mapPixels.data()),
        m_mapBitmapWidth,
        m_mapBitmapHeight,
        m_mapBitmapWidth * static_cast<int>(sizeof(u32)),
        QImage::Format_ARGB32);
    if (sourceImage.isNull()) {
        return false;
    }

    QImage composed(kMapWidth, kMapHeight, QImage::Format_ARGB32_Premultiplied);
    composed.fill(QColor(GetRValue(uitheme::kShadow), GetGValue(uitheme::kShadow), GetBValue(uitheme::kShadow)));

    QRect targetRect(0, 0, kMapWidth, kMapHeight);
    QRect sourceRect(0, 0, sourceImage.width(), sourceImage.height());
    const float zoom = (std::max)(kMinZoom, (std::min)(kMaxZoom, g_windowMgr.m_miniMapZoomFactor));
    const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode();
    RECT attrArea{ 0, 0, 0, 0 };
    if (gameMode && gameMode->m_world) {
        attrArea = gameMode->m_world->m_rootNode.m_attrArea;
    }

    if (zoom > kMinZoom + 0.001f) {
        const float viewAspect = static_cast<float>(kMapWidth) / static_cast<float>(kMapHeight);
        const float bitmapAspect = static_cast<float>(sourceImage.width()) / static_cast<float>(sourceImage.height());
        int srcWidth = 0;
        int srcHeight = 0;
        if (bitmapAspect > viewAspect) {
            srcHeight = static_cast<int>(sourceImage.height() / zoom);
            srcWidth = static_cast<int>(srcHeight * viewAspect);
        } else {
            srcWidth = static_cast<int>(sourceImage.width() / zoom);
            srcHeight = static_cast<int>(srcWidth / viewAspect);
        }

        srcWidth = (std::max)(1, (std::min)(srcWidth, sourceImage.width()));
        srcHeight = (std::max)(1, (std::min)(srcHeight, sourceImage.height()));

        float centerNormX = 0.5f;
        float centerNormY = 0.5f;
        const int attrWidth = attrArea.right - attrArea.left;
        const int attrHeight = attrArea.bottom - attrArea.top;
        if (attrWidth > 0 && attrHeight > 0) {
            centerNormX = static_cast<float>(g_session.m_playerPosX - attrArea.left) / static_cast<float>(attrWidth);
            centerNormY = 1.0f - (static_cast<float>(g_session.m_playerPosY - attrArea.top) / static_cast<float>(attrHeight));
            centerNormX = (std::max)(0.0f, (std::min)(1.0f, centerNormX));
            centerNormY = (std::max)(0.0f, (std::min)(1.0f, centerNormY));
        }

        const int centerX = static_cast<int>(centerNormX * sourceImage.width());
        const int centerY = static_cast<int>(centerNormY * sourceImage.height());
        int srcLeft = centerX - srcWidth / 2;
        int srcTop = centerY - srcHeight / 2;
        srcLeft = (std::max)(0, (std::min)(srcLeft, sourceImage.width() - srcWidth));
        srcTop = (std::max)(0, (std::min)(srcTop, sourceImage.height() - srcHeight));
        sourceRect = QRect(srcLeft, srcTop, srcWidth, srcHeight);
    } else {
        RECT fitted = FitRectPreservingAspect(RECT{ 0, 0, kMapWidth, kMapHeight }, sourceImage.width(), sourceImage.height());
        targetRect = QRect(fitted.left, fitted.top, fitted.right - fitted.left, fitted.bottom - fitted.top);
    }

    {
        QPainter painter(&composed);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(targetRect, sourceImage, sourceRect);
    }

    *outImage = composed;
    return true;
#endif
}

bool UIRoMapWnd::BuildOverlayImageForRenderer(QImage* outImage) const
{
#if !RO_ENABLE_QT6_UI
    (void)outImage;
    return false;
#else
    if (!outImage) {
        return false;
    }
    DisplayData display{};
    QImage minimapImage;
    if (!GetDisplayDataForQt(&display) || !BuildQtMinimapImage(&minimapImage)) {
        return false;
    }
    const auto themeColor = [](COLORREF color) {
        return QColor(GetRValue(color), GetGValue(color), GetBValue(color));
    };
    QImage composed(m_w, m_h, QImage::Format_ARGB32_Premultiplied);
    composed.fill(Qt::transparent);
    QPainter painter(&composed);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setPen(QPen(themeColor(uitheme::kFrame), 1));
    painter.setBrush(themeColor(uitheme::kPanelBackground));
    painter.drawRoundedRect(QRectF(0.5, 0.5, m_w - 1, m_h - 1), kWindowCornerRadius, kWindowCornerRadius);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(3.5, 3.5, m_w - 7, m_h - 7),
        (std::max)(0, kWindowCornerRadius - 2), (std::max)(0, kWindowCornerRadius - 2));
    painter.drawLine(kBodyInset, 28, m_w - kBodyInset, 28);

    const QRect mapRect(kBodyInset, kMapTop, kMapWidth, kMapHeight);
    painter.drawImage(mapRect, minimapImage);
    painter.save();
    painter.setClipRect(mapRect.adjusted(1, 1, -1, -1));
    for (const DisplayMarker& marker : display.markers) {
        painter.setPen(Qt::black);
        painter.setBrush(themeColor(static_cast<COLORREF>(marker.color)));
        painter.drawEllipse(QPointF(marker.x - m_x, marker.y - m_y), marker.radius, marker.radius);
    }
    if (display.playerVisible) {
        painter.save();
        painter.translate(display.playerX - m_x, display.playerY - m_y);
        painter.rotate(180 - (display.playerDirection & 7) * 45);
        QPainterPath arrow;
        arrow.moveTo(0, -9);
        arrow.lineTo(8, 6);
        arrow.lineTo(0, 3);
        arrow.lineTo(-8, 6);
        arrow.closeSubpath();
        painter.setPen(Qt::black);
        painter.setBrush(Qt::white);
        painter.drawPath(arrow);
        painter.restore();
    }
    painter.restore();
    painter.setPen(QPen(themeColor(uitheme::kFrame), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(mapRect.x() + 0.5, mapRect.y() + 0.5, mapRect.width() - 1, mapRect.height() - 1));

    QFont font(QString::fromLatin1(uitheme::kUiFontFamily));
    font.setPixelSize(14);
    painter.setFont(font);
    painter.setPen(themeColor(uitheme::kUiText));
    const QString title = display.mapName.empty() ? QStringLiteral("Map") : QString::fromStdString(display.mapName);
    painter.drawText(QRect(kBodyInset, 4, m_w - 2 * kBodyInset, 24), Qt::AlignCenter | Qt::TextSingleLine,
        QFontMetrics(font).elidedText(title, Qt::ElideRight, m_w - 2 * kBodyInset));
    for (const DisplayZoomButton& button : display.zoomButtons) {
        const QRect rect(button.x - m_x, button.y - m_y, button.width, button.height);
        painter.setPen(QPen(themeColor(uitheme::kFrame), 1));
        painter.setBrush(themeColor(button.pressed ? uitheme::kControlHover : uitheme::kControlBackground));
        painter.drawRoundedRect(QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5), kWindowCornerRadius, kWindowCornerRadius);
        const QColor signColor = themeColor(button.enabled ? uitheme::kUiText : uitheme::kControlHover);
        const auto bars = GetZoomSignRects(RECT{ rect.x(), rect.y(), rect.x() + rect.width(), rect.y() + rect.height() });
        for (size_t index = 0; index < (button.direction > 0 ? 2u : 1u); ++index) {
            const RECT& bar = bars[index];
            painter.fillRect(QRect(bar.left, bar.top, bar.right - bar.left, bar.bottom - bar.top), signColor);
        }
    }
    painter.end();
    *outImage = composed;
    return true;
#endif
}

void UIRoMapWnd::EnsureCreated()
{
    if (!m_controlsCreated) {
        OnCreate(0, 0);
    }
}

RECT UIRoMapWnd::GetZoomButtonRect(int direction) const
{
    const int right = m_x + m_w - kBodyInset;
    const int left = direction > 0 ? right - kZoomButtonSize
        : right - 2 * kZoomButtonSize - kZoomButtonGap;
    return RECT{ left, m_y + kZoomButtonTop, left + kZoomButtonSize, m_y + kZoomButtonTop + kZoomButtonSize };
}

bool UIRoMapWnd::CanZoom(int direction) const
{
    return direction > 0 ? g_windowMgr.m_miniMapZoomFactor < kMaxZoom
        : g_windowMgr.m_miniMapZoomFactor > kMinZoom;
}

void UIRoMapWnd::DrawZoomButtons(HDC hdc, int drawX, int drawY)
{
    for (int direction : { -1, 1 }) {
        RECT rect = GetZoomButtonRect(direction);
        OffsetRect(&rect, drawX - m_x, drawY - m_y);
        uitheme::FillRoundedRect(hdc, rect, m_pressedZoomDirection == direction
            ? uitheme::kControlHover : uitheme::kControlBackground);
        uitheme::FrameRoundedRect(hdc, rect, uitheme::kFrame);
        const COLORREF signColor = CanZoom(direction) ? uitheme::kUiText : uitheme::kControlHover;
        const auto bars = GetZoomSignRects(rect);
        FillSolidRect(hdc, bars.front(), signColor);
        if (direction > 0) {
            FillSolidRect(hdc, bars.back(), signColor);
        }
    }
}

void UIRoMapWnd::ReleaseAssets()
{
    ReleaseRenderCache();
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
    m_mapPixels.clear();
    m_loadedBitmapName.clear();
    m_loadedBitmapPath.clear();
}

void UIRoMapWnd::EnsureRenderCache()
{
    if (m_w <= 0 || m_h <= 0) {
        return;
    }
    if (m_renderCacheSurface.IsValid()
        && m_renderCacheSurface.GetWidth() == m_w
        && m_renderCacheSurface.GetHeight() == m_h) {
        return;
    }

    ReleaseRenderCache();
    if (!m_renderCacheSurface.EnsureSize(m_w, m_h)) {
        return;
    }
    m_renderCacheDirty = true;
}

void UIRoMapWnd::ReleaseRenderCache()
{
    m_renderCacheSurface.Release();
    m_renderCacheDirty = true;
}

void UIRoMapWnd::InvalidateRenderCache()
{
    m_renderCacheDirty = true;
}

void UIRoMapWnd::UpdateMinimapBitmap()
{
    const std::string bitmapName = GetCurrentMinimapBitmapName();
    if (bitmapName.empty()) {
        m_mapPixels.clear();
        m_mapBitmapWidth = 0;
        m_mapBitmapHeight = 0;
        m_loadedBitmapName.clear();
        m_loadedBitmapPath.clear();
        InvalidateRenderCache();
        return;
    }

    const bool hasCachedMinimap = !m_mapPixels.empty() && m_mapBitmapWidth > 0 && m_mapBitmapHeight > 0;
    if (!m_loadedBitmapName.empty() && ToLowerAscii(m_loadedBitmapName) == ToLowerAscii(bitmapName) && hasCachedMinimap) {
        return;
    }

    const std::string resolvedPath = ResolveMinimapPath(bitmapName);
    m_mapPixels.clear();
    m_mapBitmapWidth = 0;
    m_mapBitmapHeight = 0;
    m_loadedBitmapName = bitmapName;
    m_loadedBitmapPath = resolvedPath;
    if (!resolvedPath.empty()) {
        u32* pixels = nullptr;
        if (LoadBgraPixelsFromGameData(resolvedPath.c_str(), &pixels, &m_mapBitmapWidth, &m_mapBitmapHeight) && pixels) {
            const size_t pixelCount = static_cast<size_t>(m_mapBitmapWidth) * static_cast<size_t>(m_mapBitmapHeight);
            m_mapPixels.assign(pixels, pixels + pixelCount);
            for (u32& pixel : m_mapPixels) {
                if ((pixel & 0x00FFFFFFu) == 0x00FF00FFu) {
                    pixel = 0u;
                }
            }
        }
        delete[] pixels;
    }
    InvalidateRenderCache();
    Invalidate();
}

std::string UIRoMapWnd::GetCurrentMinimapBitmapName() const
{
    if (const CGameMode* gameMode = g_modeMgr.GetCurrentGameMode()) {
        if (gameMode->m_minimapBmpName[0] != '\0') {
            return NormalizeSlash(gameMode->m_minimapBmpName);
        }
    }

    if (g_session.m_curMap[0] == '\0') {
        return std::string();
    }

    char buffer[64] = {};
    std::strncpy(buffer, g_session.m_curMap, sizeof(buffer) - 1);
    char* dot = std::strrchr(buffer, '.');
    if (dot) {
        *dot = '\0';
    }

    char bitmapName[72] = {};
    std::snprintf(bitmapName, sizeof(bitmapName), "%s.bmp", buffer);
    return bitmapName;
}

unsigned long long UIRoMapWnd::BuildVisualStateToken() const
{
    unsigned long long hash = 1469598103934665603ull;
    const auto mixValue = [&hash](unsigned long long value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    const auto mixString = [&mixValue](const std::string& value) {
        for (unsigned char ch : value) {
            mixValue(static_cast<unsigned long long>(ch));
        }
        mixValue(0xFFull);
    };

    mixValue(static_cast<unsigned long long>(m_show));
    mixValue(static_cast<unsigned long long>(m_x));
    mixValue(static_cast<unsigned long long>(m_y));
    mixValue(static_cast<unsigned long long>(m_w));
    mixValue(static_cast<unsigned long long>(m_h));
    mixValue(static_cast<unsigned long long>(g_windowMgr.m_isDrawCompass));
    mixValue(static_cast<unsigned long long>(m_pressedZoomDirection));
    // The zoomed crop follows the player, so moving must refresh the Qt image.
    mixValue(static_cast<unsigned long long>(g_session.m_playerPosX));
    mixValue(static_cast<unsigned long long>(g_session.m_playerPosY));
    mixValue(static_cast<unsigned long long>(g_windowMgr.m_miniMapZoomFactor * 100.0f));
    mixString(GetCurrentMinimapBitmapName());
    mixString(m_loadedBitmapPath);

    return hash;
}
