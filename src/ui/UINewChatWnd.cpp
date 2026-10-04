#include "UINewChatWnd.h"

#include "UIShortCutWnd.h"
#include "UIExperienceBarWnd.h"
#include "UIWindowMgr.h"
#include "ui/UiTheme.h"
#include "UiScale.h"
#include "core/SettingsIni.h"
#include "gamemode/GameMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "render/DC.h"

#if RO_ENABLE_QT6_UI
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QString>
#include <QTextLayout>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <windows.h>

#if RO_PLATFORM_WINDOWS
#pragma comment(lib, "msimg32.lib")
#endif

namespace {
constexpr char kChatWndSection[] = "ChatWnd";
constexpr char kChatFontSizeValue[] = "FontPixelSize";
constexpr char kChatWindowOpacityValue[] = "OpacityPercent";
constexpr char kChatWindowWidthValue[] = "WindowWidth";
constexpr char kChatWindowHeightValue[] = "WindowHeight";
constexpr char kChatActiveTabValue[] = "ActiveTab";
constexpr char kChatLayoutVersionValue[] = "LayoutVersion";
constexpr int kChatLayoutVersion = 2;

constexpr size_t kMaxChatLines = 50;
constexpr int kChatDefaultWindowWidth = 600;
constexpr int kChatTabBarHeight = 34;
constexpr int kChatLineHeight = 19;
constexpr int kChatInputHeight = 32;
constexpr int kChatWhisperInputWidth = 108;
constexpr int kChatInputGap = 0;
constexpr int kChatPanelPadding = 10;
constexpr int kChatMessageGap = 2;
constexpr int kChatVisibleLineCount = 6;
constexpr int kChatHistoryHeight =
    (kChatLineHeight * kChatVisibleLineCount) + (kChatMessageGap * (kChatVisibleLineCount - 1)) + 8;
constexpr int kChatDefaultWindowHeight = kChatTabBarHeight + kChatHistoryHeight + kChatInputHeight + (kChatPanelPadding * 2) + 1;
constexpr int kChatMinVisibleLineCount = 4;
constexpr int kChatMinHistoryHeight =
    (kChatLineHeight * kChatMinVisibleLineCount) + (kChatMessageGap * (kChatMinVisibleLineCount - 1)) + 8;
constexpr int kChatWindowMinWidth = 360;
constexpr int kChatWindowMinHeight = kChatTabBarHeight + kChatMinHistoryHeight + kChatInputHeight + (kChatPanelPadding * 2) + 1;
constexpr int kChatResizeBorder = 6;
constexpr int kChatScrollbarWidth = 8;
constexpr int kChatScrollbarGap = 4;
constexpr int kChatTabWidth = 104;
constexpr int kChatConfigPanelWidth = 336;
constexpr int kChatConfigPanelHeight = 204;
constexpr int kChatConfigButtonHeight = 28;
constexpr int kChatSliderTrackHeight = 8;
constexpr int kChatSliderThumbWidth = 14;
constexpr int kChatFontSizeMin = 11;
constexpr int kChatFontSizeMax = 18;
constexpr int kChatFontSizeDefault = 14;
constexpr int kChatWindowOpacityMin = 20;
constexpr int kChatWindowOpacityMax = 100;
constexpr int kChatWindowOpacityDefault = 50;
constexpr size_t kMaxWhisperTargetChars = 24;
constexpr size_t kMaxInputChars = 180;
constexpr size_t kMaxInputHistory = 5;
constexpr int kChatWheelScrollLines = 3;
constexpr int kChatTabCount = 3;

enum ChatChannelId {
    ChatChannel_Normal = 0,
    ChatChannel_Player = 1,
    ChatChannel_Whisper = 2,
    ChatChannel_Party = 3,
    ChatChannel_Broadcast = 4,
    ChatChannel_Battlefield = 5,
    ChatChannel_System = 6,
    ChatChannel_Guild = 7,
};

enum ChatResizeEdge {
    ChatResizeEdge_None = 0,
    ChatResizeEdge_Left = 1 << 0,
    ChatResizeEdge_Top = 1 << 1,
    ChatResizeEdge_Right = 1 << 2,
    ChatResizeEdge_Bottom = 1 << 3,
};

struct ChatTabPreset {
    const char* label;
    u32 channelMask;
};

constexpr u32 ChatChannelBit(int channel)
{
    return channel >= 0 && channel < 32 ? (1u << channel) : 0u;
}

// General includes every channel except the two group chats.
constexpr std::array<ChatTabPreset, kChatTabCount> kChatTabPresets = {{
    { "General", ~(ChatChannelBit(ChatChannel_Party) | ChatChannelBit(ChatChannel_Guild)) },
    { "Party", ChatChannelBit(ChatChannel_Party) },
    { "Guild", ChatChannelBit(ChatChannel_Guild) },
}};

int ClampChatWindowOpacityPercent(int value)
{
    return std::clamp(value, kChatWindowOpacityMin, kChatWindowOpacityMax);
}

int ClampChatFontPixelSize(int value)
{
    return std::clamp(value, kChatFontSizeMin, kChatFontSizeMax);
}

bool PointInRectXY(const RECT& rc, int x, int y)
{
    return x >= rc.left && x < rc.right && y >= rc.top && y < rc.bottom;
}

bool PointInExpandedRectXY(const RECT& rc, int x, int y, int expand)
{
    RECT expanded = {
        rc.left - expand,
        rc.top - expand,
        rc.right + expand,
        rc.bottom + expand
    };
    return PointInRectXY(expanded, x, y);
}

bool RectIsValid(const RECT& rc)
{
    return rc.right > rc.left && rc.bottom > rc.top;
}

COLORREF ToColorRef(u32 color)
{
    return RGB((color >> 16) & 0xFFu, (color >> 8) & 0xFFu, color & 0xFFu);
}

void DrawBitmapPixelsTransparent(HDC target, const shopui::BitmapPixels& bitmap, int x, int y, int width, int height)
{
    if (!target || !bitmap.IsValid() || width <= 0 || height <= 0) {
        return;
    }
    RECT dst = { x, y, x + width, y + height };
    shopui::DrawBitmapPixelsTransparent(target, bitmap, dst);
}

#if RO_ENABLE_QT6_UI
QFont BuildChatUiFont(int pixelSize, bool themeFont = false)
{
    QFont font(themeFont ? QString::fromLatin1(uitheme::kUiFontFamily) : QStringLiteral("Segoe UI"));
    font.setPixelSize(ClampChatFontPixelSize(pixelSize));
    font.setBold(themeFont);
    font.setStyleStrategy(QFont::PreferAntialias);
    return font;
}

qreal LayoutChatText(QTextLayout& layout, int width)
{
    QTextOption option;
    option.setWrapMode(QTextOption::WordWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    qreal height = 0;
    for (;;) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(width);
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
    return height;
}

int MeasureWrappedTextHeightQt(const std::string& text, int width, int pixelSize)
{
    if (width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

    const QFont font = BuildChatUiFont(pixelSize);
    QString label = QString::fromLocal8Bit(text.c_str());
    label.replace(QLatin1Char('\n'), QChar::LineSeparator);
    QTextLayout layout(label, font);
    return (std::max)(kChatLineHeight, static_cast<int>(std::ceil(LayoutChatText(layout, width))));
}

void DrawChatTextQt(HDC hdc, const RECT& rect, const std::string& text, COLORREF color,
    bool wrap, int pixelSize, bool themeFont = false, ChatSpeakerRange speaker = {})
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    QString label = QString::fromLocal8Bit(text.c_str());
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const QFont font = BuildChatUiFont(pixelSize, themeFont);
    if (!wrap) {
        const QFontMetrics metrics(font);
        label = metrics.elidedText(label, Qt::ElideRight, width);
    }

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
    const int flags = wrap
        ? (Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap)
        : (Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine);
    if (wrap) {
        label.replace(QLatin1Char('\n'), QChar::LineSeparator);
        QTextLayout layout(label, font);
        if (speaker.length > 0) {
            QTextLayout::FormatRange nameFormat;
            nameFormat.start = static_cast<int>(QString::fromLocal8Bit(text.data(), static_cast<int>(speaker.start)).size());
            nameFormat.length = static_cast<int>(QString::fromLocal8Bit(text.data() + speaker.start,
                static_cast<int>(speaker.length)).size());
            nameFormat.format.setForeground(QColor::fromRgb(ChatLine::kSpeakerColor));
            layout.setFormats({ nameFormat });
        }
        LayoutChatText(layout, width);
        layout.draw(&painter, QPointF(0, 0));
    } else {
        painter.drawText(QRect(0, 0, width, height), flags, label);
    }
    AlphaBlendArgbToHdc(hdc, rect.left, rect.top, width, height, pixels.data(), width, height);
}
#endif

HFONT GetChatUiFont(int pixelSize, bool themeFont = false);

int MeasureWrappedTextHeight(const std::string& text, int width, int pixelSize)
{
    if (width <= 0 || text.empty()) {
        return kChatLineHeight;
    }

#if RO_ENABLE_QT6_UI
    return MeasureWrappedTextHeightQt(text, width, pixelSize);
#else
    HDC hdc = AcquireMainWindowDrawTarget();
    if (!hdc) {
        return kChatLineHeight;
    }
    RECT calcRect{ 0, 0, width, 0 };
    const int savedDc = SaveDC(hdc);
    SelectObject(hdc, GetChatUiFont(pixelSize));
    DrawTextA(hdc, text.c_str(), -1, &calcRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_CALCRECT | DT_NOPREFIX);
    RestoreDC(hdc, savedDc);
    ReleaseMainWindowDrawTarget(hdc);
    const int measuredHeight = calcRect.bottom - calcRect.top;
    return (std::max)(kChatLineHeight, measuredHeight);
#endif
}

HFONT GetChatUiFont(int pixelSize, bool themeFont)
{
    static std::array<HFONT, kChatFontSizeMax + 1> s_chatUiFonts{};
    static std::array<HFONT, kChatFontSizeMax + 1> s_chatThemeFonts{};
    const int clampedPixelSize = ClampChatFontPixelSize(pixelSize);
    auto& fonts = themeFont ? s_chatThemeFonts : s_chatUiFonts;
    HFONT& cachedFont = fonts[static_cast<size_t>(clampedPixelSize)];
    if (!cachedFont) {
        cachedFont = CreateFontA(
            -clampedPixelSize,
            0,
            0,
            0,
            themeFont ? FW_BOLD : FW_NORMAL,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE,
            themeFont ? uitheme::kUiFontFamily : "Segoe UI");
    }
    return cachedFont;
}

const shopui::BitmapPixels& GetHistoryPanelPattern(int opacityPercent)
{
    static shopui::BitmapPixels s_pattern;
    static int s_opacityPercent = -1;
    if (s_pattern.IsValid() && s_opacityPercent == opacityPercent) {
        return s_pattern;
    }
    s_opacityPercent = opacityPercent;

    constexpr int kPatternSize = 4;
    constexpr int thresholds[kPatternSize][kPatternSize] = {
        { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 }
    };
    s_pattern.width = kPatternSize;
    s_pattern.height = kPatternSize;
    s_pattern.pixels.resize(static_cast<size_t>(kPatternSize) * kPatternSize);

    const COLORREF navy = uitheme::kPanelBackground;
    const u32 background = 0xFF000000u | (static_cast<u32>(GetRValue(navy)) << 16)
        | (static_cast<u32>(GetGValue(navy)) << 8) | GetBValue(navy);
    const u32 transparent = 0x00000000u;
    for (int y = 0; y < kPatternSize; ++y) {
        for (int x = 0; x < kPatternSize; ++x) {
            // Native rendering uses a dithered fill; 50% leaves exactly half the pixels clear.
            const bool useTransparentPixel = thresholds[y][x] * 100 >= opacityPercent * 16;
            s_pattern.pixels[static_cast<size_t>(y) * kPatternSize + x] = useTransparentPixel ? transparent : background;
        }
    }

    return s_pattern;
}

void FillRectStippled(HDC target, const RECT& rect, int opacityPercent)
{
    if (!target || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const shopui::BitmapPixels& pattern = GetHistoryPanelPattern(opacityPercent);
    if (!pattern.IsValid()) {
        HBRUSH brush = CreateSolidBrush(uitheme::kPanelBackground);
        FillRect(target, &rect, brush);
        DeleteObject(brush);
        return;
    }

    constexpr int kPatternSize = 4;
    for (int y = rect.top; y < rect.bottom; y += kPatternSize) {
        for (int x = rect.left; x < rect.right; x += kPatternSize) {
            const int drawWidth = (std::min)(kPatternSize, static_cast<int>(rect.right - x));
            const int drawHeight = (std::min)(kPatternSize, static_cast<int>(rect.bottom - y));
            DrawBitmapPixelsTransparent(target, pattern, x, y, drawWidth, drawHeight);
        }
    }
}

RECT GetChatLogicalClientRect()
{
    RECT clientRect{ 0, 0, WINDOW_WIDTH > 0 ? WINDOW_WIDTH : 640, WINDOW_HEIGHT > 0 ? WINDOW_HEIGHT : 480 };
    if (g_hMainWnd) {
        GetClientRect(g_hMainWnd, &clientRect);
    }

    const float scaleFactor = IsQtUiRuntimeEnabled() ? GetConfiguredUiScaleFactor() : 1.0f;
    const float normalizedScale = scaleFactor > 0.0f ? scaleFactor : 1.0f;
    const int rawWidth = clientRect.right - clientRect.left;
    const int rawHeight = clientRect.bottom - clientRect.top;
    // Use the current client area for placement, dragging and resizing. The
    // configured startup resolution can be smaller than a maximized window.
    const int logicalWidth = (std::max)(1, static_cast<int>(std::floor(static_cast<float>(rawWidth) / normalizedScale)));
    const int logicalHeight = (std::max)(1, static_cast<int>(std::floor(static_cast<float>(rawHeight) / normalizedScale)));

    RECT logicalRect{};
    logicalRect.left = 0;
    logicalRect.top = 0;
    logicalRect.right = logicalWidth;
    logicalRect.bottom = logicalHeight;
    return logicalRect;
}

RECT GetChatAvailableClientRect()
{
    RECT rect = GetChatLogicalClientRect();
    // Reserve the EXP strip, skill bar and chat gap even when chat is created first.
    const int shortcutHeight = UIShortCutWnd::GetWindowHeightForClientWidth(rect.right - rect.left);
    rect.bottom = (std::max)(static_cast<int>(rect.top) + 1,
        static_cast<int>(rect.bottom) - UIExperienceBarWnd::kReservedHeight
        - shortcutHeight - UIShortCutWnd::kChatGap);
    return rect;
}

void ClampChatWindowPosition(int* x, int* y, int w, int h)
{
    if (!x || !y) {
        return;
    }

    const RECT logicalClientRect = GetChatAvailableClientRect();
    const int minX = static_cast<int>(logicalClientRect.left);
    const int minY = static_cast<int>(logicalClientRect.top);
    const int maxX = (std::max)(minX, static_cast<int>(logicalClientRect.right) - (std::max)(1, w));
    const int maxY = (std::max)(minY, static_cast<int>(logicalClientRect.bottom) - (std::max)(1, h));
    *x = std::clamp(*x, minX, maxX);
    *y = std::clamp(*y, minY, maxY);
}

void GetChatWindowSizeLimits(int* outMinWidth, int* outMinHeight, int* outMaxWidth, int* outMaxHeight)
{
    const RECT logicalClientRect = GetChatAvailableClientRect();
    const int availableWidth = (std::max)(1, static_cast<int>(logicalClientRect.right - logicalClientRect.left));
    const int availableHeight = (std::max)(1, static_cast<int>(logicalClientRect.bottom - logicalClientRect.top));
    if (outMaxWidth) {
        *outMaxWidth = availableWidth;
    }
    if (outMaxHeight) {
        *outMaxHeight = availableHeight;
    }
    if (outMinWidth) {
        *outMinWidth = (std::min)(kChatWindowMinWidth, availableWidth);
    }
    if (outMinHeight) {
        *outMinHeight = (std::min)(kChatWindowMinHeight, availableHeight);
    }
}

int GetChatResizeEdgesForPoint(const ChatLayoutRects& layout, int x, int y)
{
    if (!PointInExpandedRectXY(layout.panel, x, y, kChatResizeBorder)) {
        return ChatResizeEdge_None;
    }

    int resizeEdges = ChatResizeEdge_None;
    if (x < layout.panel.left + kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Left;
    } else if (x >= layout.panel.right - kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Right;
    }

    if (y < layout.panel.top + kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Top;
    } else if (y >= layout.panel.bottom - kChatResizeBorder) {
        resizeEdges |= ChatResizeEdge_Bottom;
    }

    return resizeEdges;
}

ChatLayoutRects BuildChatLayoutRects(int x, int y, int w, int h)
{
    ChatLayoutRects rects{};
    rects.panel = { x, y, x + w, y + h };

    rects.header = { x, y, x + w, y + kChatTabBarHeight };
    rects.body = { x, rects.header.bottom, x + w, y + h };
    const int tabWidth = (std::min)(kChatTabWidth, (std::max)(0, w) / kChatTabCount);
    for (int index = 0; index < kChatTabCount; ++index) {
        rects.tabs[static_cast<size_t>(index)] = {
            x + index * tabWidth, y, x + (index + 1) * tabWidth, rects.header.bottom + 1
        };
    }

    const int inputLeft = x + 1;
    const int inputRight = (std::max)(inputLeft, x + w - 1);
    const int inputTop = (std::max)(static_cast<int>(rects.body.top), y + h - kChatInputHeight - 1);
    const int inputBottom = y + h - 1;
    const int totalInputWidth = inputRight - inputLeft;
    const int whisperWidth = (std::min)(kChatWhisperInputWidth, totalInputWidth / 3);
    rects.whisperInput = { inputLeft, inputTop, inputLeft + whisperWidth, inputBottom };
    rects.messageInput = { rects.whisperInput.right + kChatInputGap, inputTop, inputRight, inputBottom };
    rects.history = {
        x + kChatPanelPadding, rects.body.top + kChatPanelPadding,
        x + w - kChatPanelPadding, inputTop - kChatPanelPadding
    };

    const RECT clientRect = GetChatAvailableClientRect();
    rects.configPanel = {
        rects.panel.right + 10,
        rects.panel.top,
        rects.panel.right + 10 + kChatConfigPanelWidth,
        rects.panel.top + kChatConfigPanelHeight
    };
    if (rects.configPanel.right > clientRect.right - 8) {
        rects.configPanel.left = (std::max)(8, static_cast<int>(rects.panel.right) - kChatConfigPanelWidth);
        rects.configPanel.right = rects.configPanel.left + kChatConfigPanelWidth;
    }
    if (rects.configPanel.bottom > clientRect.bottom - 8) {
        rects.configPanel.top = (std::max)(8, static_cast<int>(clientRect.bottom) - 8 - kChatConfigPanelHeight);
        rects.configPanel.bottom = rects.configPanel.top + kChatConfigPanelHeight;
    }

    const int editorLeft = rects.configPanel.left + 12;
    rects.fontValue = {
        editorLeft,
        rects.configPanel.top + 72,
        rects.configPanel.right - 12,
        rects.configPanel.top + 72 + kChatConfigButtonHeight
    };
    rects.fontMinusButton = {
        rects.configPanel.right - 76,
        rects.fontValue.top,
        rects.configPanel.right - 48,
        rects.fontValue.bottom
    };
    rects.fontPlusButton = {
        rects.configPanel.right - 40,
        rects.fontValue.top,
        rects.configPanel.right - 12,
        rects.fontValue.bottom
    };

    rects.transparencyValue = {
        editorLeft,
        rects.configPanel.top + 128,
        rects.configPanel.right - 12,
        rects.configPanel.top + 128 + kChatConfigButtonHeight
    };
    rects.transparencyTrack = {
        rects.configPanel.left + 22,
        rects.configPanel.top + 168,
        rects.configPanel.right - 22,
        rects.configPanel.top + 168 + kChatSliderTrackHeight
    };
    rects.transparencyThumb = {
        rects.transparencyTrack.left,
        rects.transparencyTrack.top - 5,
        rects.transparencyTrack.left + kChatSliderThumbWidth,
        rects.transparencyTrack.bottom + 5
    };

    return rects;
}

} // namespace

ChatSpeakerRange ChatLine::GetSpeakerRange() const
{
    if (channel == ChatChannel_Whisper) {
        // Whispers arrive as "(From Name) message", rather than "Name : message".
        constexpr size_t nameStart = 6;
        if (text.compare(0, nameStart, "(From ") == 0) {
            const size_t nameEnd = text.find(") ", nameStart);
            if (nameEnd != std::string::npos && nameEnd > nameStart) {
                return { nameStart, nameEnd - nameStart };
            }
        }
        return {};
    }

    if (channel != ChatChannel_Normal && channel != ChatChannel_Player
        && channel != ChatChannel_Party && channel != ChatChannel_Guild) {
        return {};
    }
    const size_t separator = text.find(':');
    if (separator == std::string::npos) {
        return {};
    }
    const size_t nameStart = text.find_first_not_of(" \t");
    const size_t nameEnd = separator > 0 ? text.find_last_not_of(" \t", separator - 1) : std::string::npos;
    if (nameStart == std::string::npos || nameEnd == std::string::npos
        || nameStart > nameEnd || nameEnd - nameStart + 1 > kMaxWhisperTargetChars) {
        return {};
    }
    return { nameStart, nameEnd - nameStart + 1 };
}

u32 ChatLine::GetDisplayColor() const
{
    return GetSpeakerRange().length > 0 ? 0x00FFFFFFu : color;
}

UINewChatWnd::UINewChatWnd()
    : m_lastDrawTick(0),
      m_activeInputField(InputField_None),
      m_historyBrowseIndex(-1),
      m_scrollLineOffset(0),
      m_firstVisibleLineIndex(0),
    m_activeTab(0),
    m_fontPixelSize(kChatFontSizeDefault),
    m_windowOpacityPercent(kChatWindowOpacityDefault),
    m_configVisible(0),
      m_dragStartGlobalX(0),
      m_dragStartGlobalY(0),
      m_dragStartWindowX(0),
      m_dragStartWindowY(0),
      m_dragStartWindowW(0),
      m_dragStartWindowH(0),
      m_dragArmed(0),
      m_isDragging(0),
      m_resizeEdges(ChatResizeEdge_None),
      m_transparencyDragActive(0),
      m_dockedAboveShortCut(true)
{
    int initialWidth = kChatDefaultWindowWidth;
    int initialHeight = kChatDefaultWindowHeight;
    LoadSettings(&initialWidth, &initialHeight);

    Create(initialWidth, initialHeight);

    // Open chat left-aligned above the skill bar, leaving a gap between them.
    const RECT logicalClientRect = GetChatAvailableClientRect();
    int defaultX = logicalClientRect.left;
    int defaultY = logicalClientRect.bottom - initialHeight;
    ClampChatWindowPosition(&defaultX, &defaultY, initialWidth, initialHeight);

    Move(defaultX, defaultY);
}

UINewChatWnd::~UINewChatWnd() = default;

void UINewChatWnd::LoadSettings(int* outWindowWidth, int* outWindowHeight)
{
    int storedValue = 0;
    if (TryLoadSettingsIniInt(kChatWndSection, kChatFontSizeValue, &storedValue)) {
        m_fontPixelSize = ClampChatFontPixelSize(storedValue);
    }
    // Migrate the old four-tab layout once, including its size and opacity.
    int storedVersion = 0;
    const bool currentLayout = TryLoadSettingsIniInt(kChatWndSection, kChatLayoutVersionValue, &storedVersion)
        && storedVersion >= kChatLayoutVersion;
    int storedWidth = kChatDefaultWindowWidth;
    int storedHeight = kChatDefaultWindowHeight;
    if (currentLayout) {
        if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowOpacityValue, &storedValue)) {
            m_windowOpacityPercent = ClampChatWindowOpacityPercent(storedValue);
        }
        if (TryLoadSettingsIniInt(kChatWndSection, kChatActiveTabValue, &storedValue)) {
            m_activeTab = std::clamp(storedValue, 0, kChatTabCount - 1);
        }
        if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowWidthValue, &storedValue)) {
            storedWidth = storedValue;
        }
        if (TryLoadSettingsIniInt(kChatWndSection, kChatWindowHeightValue, &storedValue)) {
            storedHeight = storedValue;
        }
    }

    int minWidth = kChatWindowMinWidth;
    int minHeight = kChatWindowMinHeight;
    int maxWidth = kChatDefaultWindowWidth;
    int maxHeight = kChatDefaultWindowHeight;
    GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);
    storedWidth = std::clamp(storedWidth, minWidth, maxWidth);
    storedHeight = std::clamp(storedHeight, minHeight, maxHeight);
    if (outWindowWidth) {
        *outWindowWidth = storedWidth;
    }
    if (outWindowHeight) {
        *outWindowHeight = storedHeight;
    }
}

void UINewChatWnd::SaveSettings() const
{
    SaveSettingsIniInt(kChatWndSection, kChatFontSizeValue, m_fontPixelSize);
    SaveSettingsIniInt(kChatWndSection, kChatWindowOpacityValue, m_windowOpacityPercent);
    SaveSettingsIniInt(kChatWndSection, kChatWindowWidthValue, m_w);
    SaveSettingsIniInt(kChatWndSection, kChatWindowHeightValue, m_h);
    SaveSettingsIniInt(kChatWndSection, kChatActiveTabValue, m_activeTab);
    SaveSettingsIniInt(kChatWndSection, kChatLayoutVersionValue, kChatLayoutVersion);

}

void UINewChatWnd::AddChatLine(const char* text, u32 color, u8 channel, u32 tick)
{
    if (!text || *text == '\0') {
        return;
    }

    const bool wasPinnedBottom = m_scrollLineOffset == 0;

    if (m_lines.size() >= kMaxChatLines) {
        m_lines.erase(m_lines.begin());
    }

    ChatLine line{};
    line.text = text;
    line.color = color;
    line.channel = channel;
    line.tick = tick;
    m_lines.push_back(std::move(line));

    if (!wasPinnedBottom
        && (kChatTabPresets[static_cast<size_t>(m_activeTab)].channelMask & ChatChannelBit(channel)) != 0u) {
        ++m_scrollLineOffset;
    }
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

bool UINewChatWnd::HandleQtMouseDown(int x, int y)
{
    return HandlePointerDown(x, y);
}

bool UINewChatWnd::HandleQtMouseMove(int x, int y)
{
    return HandlePointerMove(x, y);
}

bool UINewChatWnd::HandleQtMouseUp(int x, int y)
{
    return HandlePointerUp(x, y);
}

bool UINewChatWnd::HandleQtWheel(int delta, int x, int y)
{
    return HandlePointerWheel(delta, x, y);
}

bool UINewChatWnd::IsQtInteractionPoint(int x, int y) const
{
    if (m_show == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    if (PointInRectXY(layout.panel, x, y) || GetChatResizeEdgesForPoint(layout, x, y) != ChatResizeEdge_None) {
        return true;
    }
    return m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y);
}

bool UINewChatWnd::IsQtMainPanelPoint(int x, int y) const
{
    if (m_show == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    return PointInRectXY(layout.panel, x, y);
}

bool UINewChatWnd::IsQtPointerCaptured() const
{
    return m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0;
}

void UINewChatWnd::ClearInputFocus()
{
    SetActiveInputField(InputField_None);
}

const std::vector<ChatLine>& UINewChatWnd::GetLines() const
{
    return m_lines;
}

const std::vector<ChatLine>& UINewChatWnd::GetVisibleLines() const
{
    return m_visibleLines;
}

const std::string& UINewChatWnd::GetWhisperTargetText() const
{
    return m_whisperTargetText;
}

const std::string& UINewChatWnd::GetInputText() const
{
    return m_inputText;
}

const std::vector<std::string>& UINewChatWnd::GetInputHistory() const
{
    return m_inputHistory;
}

ChatScrollBarState UINewChatWnd::GetScrollBarState() const
{
    ChatScrollBarState state{};
    for (const ChatLine& line : m_lines) {
        if ((kChatTabPresets[static_cast<size_t>(m_activeTab)].channelMask & ChatChannelBit(line.channel)) != 0u) {
            ++state.totalLines;
        }
    }
    state.firstVisibleLine = m_firstVisibleLineIndex;
    state.visibleLineCount = static_cast<int>(m_visibleLines.size());
    state.visible = (state.totalLines > state.visibleLineCount) ? 1 : 0;
    return state;
}

int UINewChatWnd::GetFontPixelSize() const
{
    return m_fontPixelSize;
}

int UINewChatWnd::GetWindowOpacityPercent() const
{
    return m_windowOpacityPercent;
}

bool UINewChatWnd::IsConfigVisible() const
{
    return m_configVisible != 0;
}

bool UINewChatWnd::GetConfigRectForQt(int* x, int* y, int* width, int* height) const
{
    if (!x || !y || !width || !height || m_configVisible == 0) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    *x = layout.configPanel.left;
    *y = layout.configPanel.top;
    *width = layout.configPanel.right - layout.configPanel.left;
    *height = layout.configPanel.bottom - layout.configPanel.top;
    return true;
}

int UINewChatWnd::GetTabCount() const
{
    return kChatTabCount;
}

bool UINewChatWnd::GetTabDisplay(int index, ChatTabDisplay* outData) const
{
    if (!outData || index < 0 || index >= kChatTabCount) {
        return false;
    }

    outData->id = index;
    outData->label = kChatTabPresets[static_cast<size_t>(index)].label;
    outData->active = index == m_activeTab ? 1 : 0;
    outData->channelMask = kChatTabPresets[static_cast<size_t>(index)].channelMask;
    outData->bounds = GetDisplayLayout().tabs[static_cast<size_t>(index)];
    return true;
}

ChatLayoutRects UINewChatWnd::GetDisplayLayout() const
{
    return BuildChatLayoutRects(m_x, m_y, m_w, m_h);
}

void UINewChatWnd::OnProcess()
{
    Layout();
    const auto& events = g_windowMgr.GetChatEvents();
    std::vector<ChatLine> syncedLines;
    const size_t start = events.size() > kMaxChatLines ? (events.size() - kMaxChatLines) : 0;
    syncedLines.reserve(events.size() - start);
    for (size_t i = start; i < events.size(); ++i) {
        ChatLine line{};
        line.text = events[i].text;
        line.color = events[i].color;
        line.channel = events[i].channel;
        line.tick = events[i].tick;
        syncedLines.push_back(std::move(line));
    }
    bool linesChanged = syncedLines.size() != m_lines.size();
    if (!linesChanged) {
        for (size_t i = 0; i < syncedLines.size(); ++i) {
            if (syncedLines[i].text != m_lines[i].text ||
                syncedLines[i].color != m_lines[i].color ||
                syncedLines[i].channel != m_lines[i].channel ||
                syncedLines[i].tick != m_lines[i].tick) {
                linesChanged = true;
                break;
            }
        }
    }
    if (linesChanged) {
        const bool wasPinnedBottom = m_scrollLineOffset == 0;
        const size_t oldCount = m_lines.size();
        m_lines.swap(syncedLines);
        if (!wasPinnedBottom && m_lines.size() > oldCount) {
            m_scrollLineOffset += static_cast<int>(m_lines.size() - oldCount);
        }
        ClampScrollOffset();
        RefreshVisibleLines(GetTickCount());
        Invalidate();
    }
    RefreshVisibleLines(GetTickCount());
}

void UINewChatWnd::OnDraw()
{
    if (IsQtUiRuntimeEnabled()) {
        m_lastDrawTick = GetTickCount();
        m_isDirty = 0;
        return;
    }

    if (!g_hMainWnd || m_show == 0) {
        return;
    }

    HDC hdc = AcquireDrawTarget();
    if (!hdc) {
        return;
    }

    const int savedDc = SaveDC(hdc);
    SelectObject(hdc, GetChatUiFont(m_fontPixelSize));

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    RECT historyRc = layout.history;
    RECT whisperInputRc = layout.whisperInput;
    RECT messageInputRc = layout.messageInput;
    HBRUSH frameBrush = CreateSolidBrush(uitheme::kFrame);
    HBRUSH accentBrush = CreateSolidBrush(uitheme::kAccent);
    FillRectStippled(hdc, layout.body, m_windowOpacityPercent);
    FrameRect(hdc, &layout.body, frameBrush);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));
    SelectObject(hdc, GetChatUiFont(14, true));
    for (int index = 0; index < kChatTabCount; ++index) {
        const RECT& tab = layout.tabs[static_cast<size_t>(index)];
        FillRectStippled(hdc, tab, m_windowOpacityPercent);
        FrameRect(hdc, &tab, index == m_activeTab ? accentBrush : frameBrush);
        RECT label = tab;
        DrawTextA(hdc, kChatTabPresets[static_cast<size_t>(index)].label, -1, &label,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    const int historyClipDc = SaveDC(hdc);
    IntersectClipRect(hdc, historyRc.left, historyRc.top, historyRc.right, historyRc.bottom);
    SelectObject(hdc, GetChatUiFont(m_fontPixelSize));
    SetBkMode(hdc, TRANSPARENT);
    const int textInset = 4;
    const ChatScrollBarState scrollState = GetScrollBarState();
    const int reservedScrollbarWidth = scrollState.visible ? (kChatScrollbarWidth + kChatScrollbarGap) : 0;
    const int textWidth = (historyRc.right - historyRc.left) - (textInset * 2) - reservedScrollbarWidth;
    const size_t lineCount = m_visibleLines.size();
    std::vector<int> measuredHeights(lineCount, kChatLineHeight);
    for (size_t index = 0; index < lineCount; ++index) {
        measuredHeights[index] = MeasureWrappedTextHeight(m_visibleLines[index].text, textWidth, m_fontPixelSize);
    }

    int lineY = historyRc.top + textInset;
    for (size_t index = 0; index < lineCount; ++index) {
        const ChatLine& line = m_visibleLines[index];
        const int textHeight = measuredHeights[index];
        RECT textRc = {
            historyRc.left + textInset,
            lineY,
            historyRc.right - textInset - reservedScrollbarWidth,
            lineY + textHeight
        };
        const ChatSpeakerRange speaker = line.GetSpeakerRange();
        const COLORREF textColor = ToColorRef(line.GetDisplayColor());
        SetTextColor(hdc, textColor);
#if RO_ENABLE_QT6_UI
        DrawChatTextQt(hdc, textRc, line.text, textColor, true, m_fontPixelSize, false, speaker);
#else
        DrawTextA(hdc, line.text.c_str(), -1, &textRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
        if (speaker.length > 0) {
            // Repaint the name with the same wrapping and restore any whisper prefix.
            RECT nameRc = textRc;
            const std::string namePrefix = line.text.substr(0, speaker.start + speaker.length);
            SetTextColor(hdc, ToColorRef(ChatLine::kSpeakerColor));
            DrawTextA(hdc, namePrefix.c_str(), -1, &nameRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
            if (speaker.start > 0) {
                RECT prefixRc = textRc;
                const std::string prefix = line.text.substr(0, speaker.start);
                SetTextColor(hdc, textColor);
                DrawTextA(hdc, prefix.c_str(), -1, &prefixRc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
            }
        }
#endif
        lineY += textHeight + kChatMessageGap;
    }
    RestoreDC(hdc, historyClipDc);

    if (scrollState.visible) {
        RECT trackRc = {
            historyRc.right - textInset - kChatScrollbarWidth,
            historyRc.top + textInset,
            historyRc.right - textInset,
            historyRc.bottom - textInset
        };
        FrameRect(hdc, &trackRc, frameBrush);

        const int trackHeight = trackRc.bottom - trackRc.top;
        const int visibleLineCount = (std::max)(1, scrollState.visibleLineCount);
        const int totalLines = (std::max)(visibleLineCount, scrollState.totalLines);
        const int thumbHeight = (std::max)(18, trackHeight * visibleLineCount / totalLines);
        const int maxTravel = (std::max)(0, trackHeight - thumbHeight);
        const int scrollDenom = (std::max)(1, totalLines - visibleLineCount);
        const int thumbTop = trackRc.top + (maxTravel * scrollState.firstVisibleLine) / scrollDenom;
        RECT thumbRc = { trackRc.left + 1, thumbTop, trackRc.right - 1, thumbTop + thumbHeight };
        HBRUSH thumbBrush = CreateSolidBrush(uitheme::kFrame);
        FillRect(hdc, &thumbRc, thumbBrush);
        DeleteObject(thumbBrush);
    }

    // Inputs share the body's single translucent fill; only their borders differ.
    FrameRect(hdc, &whisperInputRc, IsWhisperTargetActive() ? accentBrush : frameBrush);
    FrameRect(hdc, &messageInputRc, IsMessageInputActive() ? accentBrush : frameBrush);

    std::string whisperDrawText = m_whisperTargetText;
    COLORREF whisperTextColor = RGB(255, 255, 255);
    if (whisperDrawText.empty() && !IsWhisperTargetActive()) {
        whisperDrawText = "Whisper...";
    }
    if (IsWhisperTargetActive()) {
        whisperDrawText += '_';
    }

    std::string messageDrawText = m_inputText;
    if (IsMessageInputActive()) {
        messageDrawText += '_';
    }
    const bool messagePlaceholder = messageDrawText.empty();
    const COLORREF messageTextColor = RGB(255, 255, 255);
    if (messagePlaceholder) {
        messageDrawText = "Type a message...";
    }

    RECT whisperTextRc = { whisperInputRc.left + 10, whisperInputRc.top + 2, whisperInputRc.right - 2, whisperInputRc.bottom - 2 };
    SelectObject(hdc, GetChatUiFont(12, true));
    SetTextColor(hdc, whisperTextColor);
#if RO_ENABLE_QT6_UI
    DrawChatTextQt(hdc, whisperTextRc, whisperDrawText, whisperTextColor, false, 12, true);
#else
    DrawTextA(hdc, whisperDrawText.c_str(), -1, &whisperTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#endif

    RECT messageTextRc = { messageInputRc.left + 10, messageInputRc.top + 2, messageInputRc.right - 2, messageInputRc.bottom - 2 };
    SelectObject(hdc, GetChatUiFont(m_fontPixelSize, true));
    SetTextColor(hdc, messageTextColor);
#if RO_ENABLE_QT6_UI
    DrawChatTextQt(hdc, messageTextRc, messageDrawText, messageTextColor, false, m_fontPixelSize, true);
#else
    DrawTextA(hdc, messageDrawText.c_str(), -1, &messageTextRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
#endif

    if (m_configVisible != 0) {
        FillRectStippled(hdc, layout.configPanel, m_windowOpacityPercent);
        FrameRect(hdc, &layout.configPanel, frameBrush);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT title = { layout.configPanel.left + 12, layout.configPanel.top + 14,
            layout.configPanel.right - 12, layout.configPanel.top + 36 };
        DrawTextA(hdc, "Chat settings", -1, &title, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        RECT fontLabel = layout.fontValue;
        const std::string fontText = "Font size: " + std::to_string(m_fontPixelSize);
        DrawTextA(hdc, fontText.c_str(), -1, &fontLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        FrameRect(hdc, &layout.fontMinusButton, frameBrush);
        FrameRect(hdc, &layout.fontPlusButton, frameBrush);
        RECT minusLabel = layout.fontMinusButton;
        RECT plusLabel = layout.fontPlusButton;
        DrawTextA(hdc, "-", -1, &minusLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DrawTextA(hdc, "+", -1, &plusLabel, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        RECT opacityLabel = layout.transparencyValue;
        const std::string opacityText = "Background opacity: " + std::to_string(m_windowOpacityPercent) + "%";
        DrawTextA(hdc, opacityText.c_str(), -1, &opacityLabel, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        FrameRect(hdc, &layout.transparencyTrack, frameBrush);
        RECT thumb = layout.transparencyThumb;
        const int travel = layout.transparencyTrack.right - layout.transparencyTrack.left;
        const int offset = std::clamp(travel * (m_windowOpacityPercent - kChatWindowOpacityMin)
            / (kChatWindowOpacityMax - kChatWindowOpacityMin) - kChatSliderThumbWidth / 2,
            0, (std::max)(0, travel - kChatSliderThumbWidth));
        OffsetRect(&thumb, offset, 0);
        FillRect(hdc, &thumb, frameBrush);
    }
    DeleteObject(accentBrush);
    DeleteObject(frameBrush);

    RestoreDC(hdc, savedDc);
    ReleaseDrawTarget(hdc);

    m_lastDrawTick = GetTickCount();
    m_isDirty = 0;
}

void UINewChatWnd::RefreshVisibleLines(u32 nowTick)
{
    (void)nowTick;

    ClampScrollOffset();

    std::vector<ChatLine> nextVisible;
    m_firstVisibleLineIndex = 0;

    if (!m_lines.empty()) {
        std::vector<const ChatLine*> filteredLines;
        filteredLines.reserve(m_lines.size());
        const u32 activeMask = kChatTabPresets[static_cast<size_t>(m_activeTab)].channelMask;
        for (const ChatLine& line : m_lines) {
            if ((activeMask & ChatChannelBit(line.channel)) != 0u) {
                filteredLines.push_back(&line);
            }
        }

        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        const ChatScrollBarState scrollState = GetScrollBarState();
        const int reservedScrollbarWidth = scrollState.visible ? (kChatScrollbarWidth + kChatScrollbarGap) : 0;
        const int textWidth = (layout.history.right - layout.history.left) - 8 - reservedScrollbarWidth;
        const int availableHeight = layout.history.bottom - layout.history.top - 8;
        const int endExclusive = (std::max)(0, static_cast<int>(filteredLines.size()) - m_scrollLineOffset);
        int usedHeight = 0;
        int firstVisibleIndex = endExclusive;
        for (int index = endExclusive - 1; index >= 0; --index) {
            const int measured = MeasureWrappedTextHeight(filteredLines[static_cast<size_t>(index)]->text, textWidth, m_fontPixelSize);
            const int blockHeight = measured + ((usedHeight > 0) ? kChatMessageGap : 0);
            if (usedHeight + blockHeight > availableHeight) {
                break;
            }
            usedHeight += blockHeight;
            firstVisibleIndex = index;
        }

        if (firstVisibleIndex == endExclusive && endExclusive > 0) {
            firstVisibleIndex = endExclusive - 1;
        }

        m_firstVisibleLineIndex = (std::max)(0, firstVisibleIndex);
        nextVisible.reserve(static_cast<size_t>((std::max)(0, endExclusive - m_firstVisibleLineIndex)));
        for (int index = m_firstVisibleLineIndex; index < endExclusive; ++index) {
            nextVisible.push_back(*filteredLines[static_cast<size_t>(index)]);
        }
    }

    if (nextVisible.size() != m_visibleLines.size()) {
        Invalidate();
    } else {
        for (size_t i = 0; i < nextVisible.size(); ++i) {
            if (nextVisible[i].text != m_visibleLines[i].text ||
                nextVisible[i].color != m_visibleLines[i].color ||
                nextVisible[i].channel != m_visibleLines[i].channel) {
                Invalidate();
                break;
            }
        }
    }
    m_visibleLines.swap(nextVisible);
}

void UINewChatWnd::ClearLines()
{
    m_lines.clear();
    m_visibleLines.clear();
    m_scrollLineOffset = 0;
    m_firstVisibleLineIndex = 0;
    Invalidate();
}

bool UINewChatWnd::HandlePointerDown(int x, int y)
{
    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);

    if (m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y)) {
        if (PointInRectXY(layout.fontMinusButton, x, y)) {
            m_fontPixelSize = ClampChatFontPixelSize(m_fontPixelSize - 1);
            SaveSettings();
            RefreshVisibleLines(GetTickCount());
            Invalidate();
            return true;
        }
        if (PointInRectXY(layout.fontPlusButton, x, y)) {
            m_fontPixelSize = ClampChatFontPixelSize(m_fontPixelSize + 1);
            SaveSettings();
            RefreshVisibleLines(GetTickCount());
            Invalidate();
            return true;
        }
        if (PointInRectXY(layout.transparencyValue, x, y) || PointInRectXY(layout.transparencyTrack, x, y)) {
            m_transparencyDragActive = 1;
            UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
            return true;
        }
        return true;
    }

    const int resizeEdges = GetChatResizeEdgesForPoint(layout, x, y);
    if (resizeEdges != ChatResizeEdge_None) {
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = resizeEdges;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    if (!PointInRectXY(layout.panel, x, y)) {
        return false;
    }

    for (int index = 0; index < kChatTabCount; ++index) {
        if (PointInRectXY(layout.tabs[static_cast<size_t>(index)], x, y)) {
            SetActiveTab(index);
            return true;
        }
    }

    if (PointInRectXY(layout.whisperInput, x, y)) {
        SetActiveInputField(InputField_WhisperTarget);
        m_dragArmed = 0;
        m_isDragging = 0;
        return true;
    }

    if (PointInRectXY(layout.messageInput, x, y)) {
        SetActiveInputField(InputField_Message);
        m_dragArmed = 0;
        m_isDragging = 0;
        return true;
    }

    if (PointInRectXY(layout.header, x, y)) {
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = ChatResizeEdge_None;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    if (PointInRectXY(layout.history, x, y)) {
        SetActiveInputField(InputField_Message);
        m_dragArmed = 1;
        m_isDragging = 0;
        m_resizeEdges = ChatResizeEdge_None;
        m_dragStartGlobalX = x;
        m_dragStartGlobalY = y;
        m_dragStartWindowX = m_x;
        m_dragStartWindowY = m_y;
        m_dragStartWindowW = m_w;
        m_dragStartWindowH = m_h;
        return true;
    }

    return true;
}

bool UINewChatWnd::HandlePointerMove(int x, int y)
{
    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
        return true;
    }

    if (m_dragArmed == 0 && m_isDragging == 0) {
        return IsQtInteractionPoint(x, y);
    }

    const int dx = x - m_dragStartGlobalX;
    const int dy = y - m_dragStartGlobalY;
    if (m_isDragging == 0 && ((dx * dx) + (dy * dy)) >= 16) {
        m_isDragging = 1;
        m_dockedAboveShortCut = false;
    }

    if (m_isDragging == 0) {
        return true;
    }

    if (m_resizeEdges != ChatResizeEdge_None) {
        int minWidth = kChatWindowMinWidth;
        int minHeight = kChatWindowMinHeight;
        int maxWidth = kChatDefaultWindowWidth;
        int maxHeight = kChatDefaultWindowHeight;
        GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);

        const RECT logicalClientRect = GetChatAvailableClientRect();
        int nextX = m_dragStartWindowX;
        int nextY = m_dragStartWindowY;
        int nextW = m_dragStartWindowW;
        int nextH = m_dragStartWindowH;
        const int startRight = m_dragStartWindowX + m_dragStartWindowW;
        const int startBottom = m_dragStartWindowY + m_dragStartWindowH;

        if ((m_resizeEdges & ChatResizeEdge_Left) != 0) {
            const int minLeft = startRight - maxWidth;
            const int maxLeft = startRight - minWidth;
            nextX = std::clamp(m_dragStartWindowX + dx, minLeft, maxLeft);
            nextX = (std::max)(static_cast<int>(logicalClientRect.left), nextX);
            nextW = startRight - nextX;
        } else if ((m_resizeEdges & ChatResizeEdge_Right) != 0) {
            const int maxRight = (std::min)(static_cast<int>(logicalClientRect.right), m_dragStartWindowX + maxWidth);
            const int minRight = m_dragStartWindowX + minWidth;
            const int nextRight = std::clamp(startRight + dx, minRight, maxRight);
            nextW = nextRight - m_dragStartWindowX;
        }

        if ((m_resizeEdges & ChatResizeEdge_Top) != 0) {
            const int minTop = startBottom - maxHeight;
            const int maxTop = startBottom - minHeight;
            nextY = std::clamp(m_dragStartWindowY + dy, minTop, maxTop);
            nextY = (std::max)(static_cast<int>(logicalClientRect.top), nextY);
            nextH = startBottom - nextY;
        } else if ((m_resizeEdges & ChatResizeEdge_Bottom) != 0) {
            const int maxBottom = (std::min)(static_cast<int>(logicalClientRect.bottom), m_dragStartWindowY + maxHeight);
            const int minBottom = m_dragStartWindowY + minHeight;
            const int nextBottom = std::clamp(startBottom + dy, minBottom, maxBottom);
            nextH = nextBottom - m_dragStartWindowY;
        }

        nextW = std::clamp(nextW, minWidth, maxWidth);
        nextH = std::clamp(nextH, minHeight, maxHeight);
        ClampChatWindowPosition(&nextX, &nextY, nextW, nextH);

        const bool sizeChanged = nextW != m_w || nextH != m_h;
        if (sizeChanged) {
            Resize(nextW, nextH);
        }
        if (nextX != m_x || nextY != m_y) {
            Move(nextX, nextY);
        }
        if (sizeChanged) {
            RefreshVisibleLines(GetTickCount());
        }
        return true;
    }

    int snappedX = m_dragStartWindowX + dx;
    int snappedY = m_dragStartWindowY + dy;
    g_windowMgr.SnapWindowToNearby(this, &snappedX, &snappedY);
    ClampChatWindowPosition(&snappedX, &snappedY, m_w, m_h);
    Move(snappedX, snappedY);
    return true;
}

bool UINewChatWnd::HandlePointerUp(int x, int y)
{
    const bool hadCapture = m_dragArmed != 0 || m_isDragging != 0 || m_transparencyDragActive != 0;
    if (m_transparencyDragActive != 0) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        UpdateWindowOpacityFromPointer(x, layout.transparencyTrack);
    }
    if (m_isDragging != 0) {
        StoreInfo();
    } else if (m_dragArmed != 0 && m_resizeEdges == ChatResizeEdge_None) {
        const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
        if (PointInRectXY(layout.panel, x, y)
            && !PointInRectXY(layout.whisperInput, x, y)
            && !PointInRectXY(layout.messageInput, x, y)) {
            SetActiveInputField(InputField_Message);
        }
    }

    m_dragArmed = 0;
    m_isDragging = 0;
    m_resizeEdges = ChatResizeEdge_None;
    m_transparencyDragActive = 0;
    return hadCapture || IsQtInteractionPoint(x, y);
}

bool UINewChatWnd::HandlePointerWheel(int delta, int x, int y)
{
    if (!IsQtInteractionPoint(x, y)) {
        return false;
    }

    const ChatLayoutRects layout = BuildChatLayoutRects(m_x, m_y, m_w, m_h);
    if (m_configVisible != 0 && RectIsValid(layout.configPanel) && PointInRectXY(layout.configPanel, x, y)
        && !PointInRectXY(layout.panel, x, y)) {
        return true;
    }

    if (delta > 0) {
        AdjustScroll(kChatWheelScrollLines);
    } else if (delta < 0) {
        AdjustScroll(-kChatWheelScrollLines);
    }
    return true;
}

void UINewChatWnd::OnLBtnDown(int x, int y)
{
    HandlePointerDown(x, y);
}

void UINewChatWnd::OnMouseMove(int x, int y)
{
    HandlePointerMove(x, y);
}

void UINewChatWnd::OnLBtnUp(int x, int y)
{
    HandlePointerUp(x, y);
}

void UINewChatWnd::OnWheel(int delta)
{
    if (delta > 0) {
        AdjustScroll(kChatWheelScrollLines);
    } else if (delta < 0) {
        AdjustScroll(-kChatWheelScrollLines);
    }
}

bool UINewChatWnd::HandleKeyDown(int virtualKey)
{
    if (virtualKey == VK_ESCAPE && m_activeInputField == InputField_None && m_configVisible != 0) {
        SetConfigVisible(false);
        return true;
    }

    if (virtualKey == VK_TAB) {
        if (m_activeInputField == InputField_WhisperTarget) {
            SetActiveInputField(InputField_Message);
        } else {
            SetActiveInputField(InputField_WhisperTarget);
        }
        return true;
    }

    if (virtualKey == VK_RETURN) {
        if (m_activeInputField == InputField_None) {
            SetActiveInputField(InputField_Message);
            return true;
        }
        if (m_activeInputField == InputField_WhisperTarget) {
            SetActiveInputField(InputField_Message);
            return true;
        }
        return SubmitInput();
    }

    if ((virtualKey == VK_UP || virtualKey == VK_DOWN) && m_activeInputField == InputField_None) {
        SetActiveInputField(InputField_Message);
    }

    if (virtualKey == VK_UP && m_activeInputField == InputField_Message) {
        if (m_inputHistory.empty()) {
            return true;
        }
        if (m_historyBrowseIndex < 0) {
            m_historyDraft = m_inputText;
            m_historyBrowseIndex = 0;
        } else if (m_historyBrowseIndex + 1 < static_cast<int>(m_inputHistory.size())) {
            ++m_historyBrowseIndex;
        }
        m_inputText = m_inputHistory[static_cast<size_t>(m_historyBrowseIndex)];
        Invalidate();
        return true;
    }

    if (virtualKey == VK_DOWN && m_activeInputField == InputField_Message) {
        if (m_historyBrowseIndex < 0) {
            return true;
        }
        --m_historyBrowseIndex;
        if (m_historyBrowseIndex >= 0) {
            m_inputText = m_inputHistory[static_cast<size_t>(m_historyBrowseIndex)];
        } else {
            m_inputText = m_historyDraft;
            m_historyDraft.clear();
        }
        Invalidate();
        return true;
    }

    if (virtualKey == VK_BACK && m_activeInputField != InputField_None) {
        if (std::string* const activeBuffer = GetActiveInputBuffer(); activeBuffer && !activeBuffer->empty()) {
            if (m_historyBrowseIndex >= 0) {
                m_historyBrowseIndex = -1;
                m_historyDraft.clear();
            }
            activeBuffer->pop_back();
            Invalidate();
        }
        return true;
    }

    if (virtualKey == VK_ESCAPE && m_activeInputField != InputField_None) {
        SetActiveInputField(InputField_None);
        return true;
    }

    return false;
}

bool UINewChatWnd::HandleChar(char c)
{
    if (m_activeInputField == InputField_None) {
        return false;
    }

    if (c == '\b' || c == '\r' || c == '\n') {
        return true;
    }

    if (static_cast<unsigned char>(c) < 0x20u) {
        return false;
    }

    std::string* const activeBuffer = GetActiveInputBuffer();
    if (!activeBuffer) {
        return false;
    }

    const size_t maxChars = (m_activeInputField == InputField_WhisperTarget) ? kMaxWhisperTargetChars : kMaxInputChars;
    if (activeBuffer->size() >= maxChars) {
        return true;
    }

    if (m_historyBrowseIndex >= 0) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    *activeBuffer += c;
    Invalidate();
    return true;
}

bool UINewChatWnd::IsWhisperTargetActive() const
{
    return m_activeInputField == InputField_WhisperTarget;
}

bool UINewChatWnd::IsMessageInputActive() const
{
    return m_activeInputField == InputField_Message;
}

bool UINewChatWnd::IsInputActive() const
{
    return m_activeInputField != InputField_None;
}

void UINewChatWnd::Layout()
{
    int minWidth = kChatWindowMinWidth;
    int minHeight = kChatWindowMinHeight;
    int maxWidth = kChatDefaultWindowWidth;
    int maxHeight = kChatDefaultWindowHeight;
    GetChatWindowSizeLimits(&minWidth, &minHeight, &maxWidth, &maxHeight);

    const int nextWidth = std::clamp(m_w > 0 ? m_w : kChatDefaultWindowWidth, minWidth, maxWidth);
    const int nextHeight = std::clamp(m_h > 0 ? m_h : kChatDefaultWindowHeight, minHeight, maxHeight);
    const bool sizeChanged = nextWidth != m_w || nextHeight != m_h;
    if (sizeChanged) {
        Resize(nextWidth, nextHeight);
    }

    const RECT availableRect = GetChatAvailableClientRect();
    int clampedX = m_dockedAboveShortCut ? static_cast<int>(availableRect.left) : m_x;
    int clampedY = m_dockedAboveShortCut ? static_cast<int>(availableRect.bottom) - nextHeight : m_y;
    ClampChatWindowPosition(&clampedX, &clampedY, nextWidth, nextHeight);
    Move(clampedX, clampedY);
    if (sizeChanged) {
        RefreshVisibleLines(GetTickCount());
    }
}

void UINewChatWnd::AdjustScroll(int lineDelta)
{
    if (lineDelta == 0 || m_lines.empty()) {
        return;
    }

    m_scrollLineOffset += lineDelta;
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::ClampScrollOffset()
{
    int filteredCount = 0;
    const u32 activeMask = kChatTabPresets[static_cast<size_t>(m_activeTab)].channelMask;
    for (const ChatLine& line : m_lines) {
        if ((activeMask & ChatChannelBit(line.channel)) != 0u) {
            ++filteredCount;
        }
    }
    const int maxOffset = filteredCount > 0 ? filteredCount - 1 : 0;
    m_scrollLineOffset = std::clamp(m_scrollLineOffset, 0, maxOffset);
}

void UINewChatWnd::SetActiveTab(int index)
{
    const int clampedIndex = std::clamp(index, 0, kChatTabCount - 1);
    if (m_activeTab == clampedIndex) {
        return;
    }

    m_activeTab = clampedIndex;
    m_scrollLineOffset = 0;
    SaveSettings();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::SetConfigVisible(bool visible)
{
    const int nextVisible = visible ? 1 : 0;
    if (m_configVisible == nextVisible) {
        return;
    }

    m_configVisible = nextVisible;
    if (m_configVisible == 0) {
        m_transparencyDragActive = 0;
    }
    Invalidate();
}

void UINewChatWnd::SetWindowOpacityPercent(int value)
{
    const int clampedValue = ClampChatWindowOpacityPercent(value);
    if (m_windowOpacityPercent == clampedValue) {
        return;
    }

    m_windowOpacityPercent = clampedValue;
    SaveSettings();
    Invalidate();
}

void UINewChatWnd::UpdateWindowOpacityFromPointer(int x, const RECT& trackRect)
{
    const int trackWidth = trackRect.right - trackRect.left;
    if (trackWidth <= 0) {
        return;
    }

    const int clampedX = std::clamp(x, static_cast<int>(trackRect.left), static_cast<int>(trackRect.right));
    const double t = static_cast<double>(clampedX - trackRect.left) / static_cast<double>(trackWidth);
    const int value = static_cast<int>(std::lround(kChatWindowOpacityMin + t * (kChatWindowOpacityMax - kChatWindowOpacityMin)));
    SetWindowOpacityPercent(value);
}

void UINewChatWnd::SetActiveInputField(ActiveInputField field)
{
    if (m_activeInputField == field) {
        return;
    }

    m_activeInputField = field;
    if (m_activeInputField != InputField_Message) {
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
    }
    Invalidate();
}

std::string* UINewChatWnd::GetActiveInputBuffer()
{
    switch (m_activeInputField) {
    case InputField_WhisperTarget:
        return &m_whisperTargetText;
    case InputField_Message:
        return &m_inputText;
    default:
        return nullptr;
    }
}

bool UINewChatWnd::SubmitInput()
{
    std::string text = m_inputText;
    if (text.empty()) {
        SetActiveInputField(InputField_None);
        return true;
    }

    const size_t firstNonSpace = text.find_first_not_of(" \t\r\n");
    if (firstNonSpace == std::string::npos) {
        m_inputText.clear();
        Invalidate();
        return true;
    }

    const size_t lastNonSpace = text.find_last_not_of(" \t\r\n");
    text = text.substr(firstNonSpace, lastNonSpace - firstNonSpace + 1);

    std::string whisperTarget = m_whisperTargetText;
    const size_t targetFirstNonSpace = whisperTarget.find_first_not_of(" \t\r\n");
    if (targetFirstNonSpace == std::string::npos) {
        whisperTarget.clear();
    } else {
        const size_t targetLastNonSpace = whisperTarget.find_last_not_of(" \t\r\n");
        whisperTarget = whisperTarget.substr(targetFirstNonSpace, targetLastNonSpace - targetFirstNonSpace + 1);
    }

    const msgresult_t sent = g_modeMgr.SendMsg(CGameMode::GameMsg_SubmitChat,
        reinterpret_cast<msgparam_t>(text.c_str()),
        reinterpret_cast<msgparam_t>(whisperTarget.c_str()),
        0);
    if (sent != 0) {
        AddInputHistory(text);
        m_inputText.clear();
        m_historyBrowseIndex = -1;
        m_historyDraft.clear();
        SetActiveInputField(InputField_Message);
        Invalidate();
        return true;
    }

    return false;
}

void UINewChatWnd::AddInputHistory(const std::string& text)
{
    if (text.empty()) {
        return;
    }

    auto existing = std::find(m_inputHistory.begin(), m_inputHistory.end(), text);
    if (existing != m_inputHistory.end()) {
        m_inputHistory.erase(existing);
    }
    m_inputHistory.insert(m_inputHistory.begin(), text);
    if (m_inputHistory.size() > kMaxInputHistory) {
        m_inputHistory.resize(kMaxInputHistory);
    }
}

void UINewChatWnd::RestorePersistentState(const std::vector<std::string>& inputHistory,
    const std::string& whisperTargetText,
    const std::string& inputText,
    int activeInputField,
    int scrollLineOffset)
{
    m_inputHistory = inputHistory;
    if (m_inputHistory.size() > kMaxInputHistory) {
        m_inputHistory.resize(kMaxInputHistory);
    }
    m_whisperTargetText = whisperTargetText.substr(0, kMaxWhisperTargetChars);
    m_inputText = inputText;
    switch (activeInputField) {
    case InputField_WhisperTarget:
        m_activeInputField = InputField_WhisperTarget;
        break;
    case InputField_Message:
        m_activeInputField = InputField_Message;
        break;
    default:
        m_activeInputField = InputField_None;
        break;
    }
    m_historyBrowseIndex = -1;
    m_historyDraft.clear();
    m_scrollLineOffset = scrollLineOffset;
    ClampScrollOffset();
    RefreshVisibleLines(GetTickCount());
    Invalidate();
}

void UINewChatWnd::StoreInfo()
{
    SaveSettings();
}
