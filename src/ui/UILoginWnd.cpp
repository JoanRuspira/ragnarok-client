#include "UILoginWnd.h"

#include "core/SettingsIni.h"
#include "gamemode/LoginMode.h"
#include "gamemode/Mode.h"
#include "main/WinMain.h"
#include "qtui/QtUiRuntime.h"
#include "ui/UiScale.h"
#include "ui/UiTheme.h"
#include "ui/UIWindowMgr.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <iterator>

namespace {

// Keep the field geometry in sync with the login panel in GameOverlay.qml.
constexpr int kPanelWidth = 320;
constexpr int kPanelHeight = 244;
constexpr int kFieldLeft = 24;
constexpr int kLoginFieldTop = 76;
constexpr int kPasswordFieldTop = 140;
constexpr int kFieldWidth = 272;
constexpr int kFieldHeight = 30;

struct LoginButtonHitArea {
    int id;
    int x;
    int y;
    int width;
    int height;
    const char* label;
};

constexpr LoginButtonHitArea kLoginButtonAreas[] = {
    { 120, 24, 188, 128, 32, "Login" },
    { 155, 168, 188, 128, 32, "Exit" },
};

int GetMenuLayoutClientExtent(int rawExtent)
{
    return IsQtUiRuntimeEnabled() ? UiScaleRawToLogicalCoordinate(rawExtent) : rawExtent;
}

bool ContainsPoint(int left, int top, int width, int height, int x, int y)
{
    return x >= left && x < left + width && y >= top && y < top + height;
}

void DrawSolidBox(HDC dc, const RECT& rect, COLORREF background, COLORREF border)
{
    uitheme::FillRoundedRect(dc, rect, background);
    uitheme::FrameRoundedRect(dc, rect, border);
}

void DrawLoginText(HDC dc, RECT rect, const char* text, COLORREF color,
    int size = 14, bool bold = false, UINT alignment = DT_LEFT,
    const char* fontFamily = uitheme::kUiFontFamily)
{
    HFONT font = CreateFontA(-size, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, fontFamily);
    HGDIOBJ oldFont = SelectObject(dc, font);
    const int oldBkMode = SetBkMode(dc, TRANSPARENT);
    const COLORREF oldTextColor = SetTextColor(dc, color);
    DrawTextA(dc, text, -1, &rect,
        alignment | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    SetTextColor(dc, oldTextColor);
    SetBkMode(dc, oldBkMode);
    SelectObject(dc, oldFont);
    DeleteObject(font);
}

class LoginEditCtrl : public UIEditCtrl {
public:
    void OnLBtnDown(int x, int y) override
    {
        if (m_parent) {
            for (UIWindow* sibling : m_parent->m_children) {
                if (auto* edit = dynamic_cast<UIEditCtrl*>(sibling)) {
                    edit->m_hasFocus = edit == this;
                    edit->Invalidate();
                }
            }
        }
        UIEditCtrl::OnLBtnDown(x, y);
        g_windowMgr.m_editWindow = this;
    }

    void OnDraw() override
    {
        if (m_show == 0 || IsQtUiRuntimeEnabled()) {
            return;
        }
        HDC dc = AcquireDrawTarget();
        if (!dc) {
            return;
        }

        const RECT rect{ m_x, m_y, m_x + m_w, m_y + m_h };
        DrawSolidBox(dc, rect, uitheme::kControlBackgroundText,
            m_hasFocus ? uitheme::kAccent : uitheme::kFrame);
        std::string text = m_maskchar != 0
            ? std::string(m_text.size(), static_cast<char>(m_maskchar)) : m_text;
        if (m_hasFocus) {
            text += '|';
        }
        const RECT textRect{ m_x + 8, m_y + 2, m_x + m_w - 8, m_y + m_h - 2 };
        DrawLoginText(dc, textRect, text.c_str(), uitheme::kWritableText,
            14, false, DT_LEFT, "Tahoma");
        ReleaseDrawTarget(dc);
    }
};

// Reuse the existing button input handling, with all visuals drawn here.
class LoginButton : public UIBitmapButton {
public:
    void OnDraw() override
    {
        if (m_show == 0 || IsQtUiRuntimeEnabled()) {
            return;
        }
        HDC dc = AcquireDrawTarget();
        if (!dc) {
            return;
        }

        const bool primary = m_id == 120;
        const COLORREF background = primary
            ? (m_state == 1 ? uitheme::kFrame : m_state == 2 ? uitheme::kAccentHover : uitheme::kAccent)
            : (m_state == 1 ? uitheme::kPanelBackground : m_state == 2 ? uitheme::kControlHover : uitheme::kControlBackground);
        const RECT rect{ m_x, m_y, m_x + m_w, m_y + m_h };
        DrawSolidBox(dc, rect, background, uitheme::kFrame);
        DrawLoginText(dc, rect, m_text.c_str(), primary ? uitheme::kControlBackground : uitheme::kUiText,
            14, true, DT_CENTER);
        ReleaseDrawTarget(dc);
    }
};

constexpr char kLoginSettingsSection[] = "Login";
constexpr char kRememberUserIdEnabledValue[] = "RememberUserIdEnabled";
constexpr char kRememberUserIdValue[] = "RememberUserId";

void StoreRememberedUserId(const char* userId, bool enabled)
{
    SaveSettingsIniInt(kLoginSettingsSection, kRememberUserIdEnabledValue, enabled ? 1 : 0);
    SaveSettingsIniString(kLoginSettingsSection, kRememberUserIdValue,
        (enabled && userId) ? std::string(userId) : std::string());
}

void SetLoginFieldFocus(UIEditCtrl* login, UIEditCtrl* password, bool focusPassword)
{
    if (login) {
        login->m_hasFocus = !focusPassword;
        login->Invalidate();
    }
    if (password) {
        password->m_hasFocus = focusPassword;
        password->Invalidate();
    }
    g_windowMgr.m_editWindow = focusPassword
        ? static_cast<UIWindow*>(password) : static_cast<UIWindow*>(login);
}

} // namespace

UILoginWnd::UILoginWnd()
    : m_controlsCreated(false), m_saveAccountChecked(false),
      m_login(nullptr), m_password(nullptr), m_hasRememberedUserIdSnapshot(false),
      m_lastRememberedUserIdEnabled(false)
{
}

UILoginWnd::~UILoginWnd()
{
    // Qt uses the edit controls for input only; native controls belong to m_children.
    if (m_login && m_login->m_parent != this) {
        delete m_login;
    }
    if (m_password && m_password->m_parent != this) {
        delete m_password;
    }
}

const char* UILoginWnd::GetLoginText() const
{
    return m_login ? m_login->GetText() : "";
}

int UILoginWnd::GetPasswordLength() const
{
    return m_password ? static_cast<int>(std::strlen(m_password->GetText())) : 0;
}

bool UILoginWnd::IsSaveAccountChecked() const
{
    return m_saveAccountChecked;
}

bool UILoginWnd::IsPasswordFocused() const
{
    return m_password && m_password->m_hasFocus;
}

int UILoginWnd::GetQtButtonCount() const
{
    return static_cast<int>(std::size(kLoginButtonAreas));
}

bool UILoginWnd::GetQtButtonDisplayForQt(int index, QtButtonDisplay* outButton) const
{
    if (!outButton || index < 0 || index >= GetQtButtonCount()) {
        return false;
    }
    const LoginButtonHitArea& button = kLoginButtonAreas[index];
    outButton->id = button.id;
    outButton->x = m_x + button.x;
    outButton->y = m_y + button.y;
    outButton->width = button.width;
    outButton->height = button.height;
    outButton->label = button.label;
    return true;
}

bool UILoginWnd::HandleQtMouseDown(int x, int y)
{
    if (m_show == 0) {
        return false;
    }
    EnsureQtLayout();
    const int localX = x - m_x;
    const int localY = y - m_y;
    if (!ContainsPoint(0, 0, m_w, m_h, localX, localY)) {
        return false;
    }
    if (ContainsPoint(kFieldLeft, kLoginFieldTop, kFieldWidth, kFieldHeight, localX, localY)) {
        SetLoginFieldFocus(m_login, m_password, false);
        return true;
    }
    if (ContainsPoint(kFieldLeft, kPasswordFieldTop, kFieldWidth, kFieldHeight, localX, localY)) {
        SetLoginFieldFocus(m_login, m_password, true);
        return true;
    }
    for (const LoginButtonHitArea& button : kLoginButtonAreas) {
        if (ContainsPoint(button.x, button.y, button.width, button.height, localX, localY)) {
            SendMsg(nullptr, 6, button.id, 0, 0);
            return true;
        }
    }
    return true;
}

void UILoginWnd::EnsureQtLayout()
{
    RECT clientRect{};
    if (g_hMainWnd && GetClientRect(g_hMainWnd, &clientRect)) {
        OnCreate(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    }
}

void UILoginWnd::LayoutControls()
{
    if (m_login) {
        m_login->Move(m_x + kFieldLeft, m_y + kLoginFieldTop);
    }
    if (m_password) {
        m_password->Move(m_x + kFieldLeft, m_y + kPasswordFieldTop);
    }
    for (UIWindow* child : m_children) {
        for (const LoginButtonHitArea& button : kLoginButtonAreas) {
            if (child->m_id == button.id) {
                child->Move(m_x + button.x, m_y + button.y);
                break;
            }
        }
    }
}

void UILoginWnd::RefreshRememberedUserIdStorage()
{
    const std::string userId = GetLoginText();
    if (m_hasRememberedUserIdSnapshot
        && m_lastRememberedUserIdEnabled == m_saveAccountChecked
        && m_lastRememberedUserId == userId) {
        return;
    }
    StoreRememberedUserId(userId.c_str(), m_saveAccountChecked);
    m_hasRememberedUserIdSnapshot = true;
    m_lastRememberedUserIdEnabled = m_saveAccountChecked;
    m_lastRememberedUserId = userId;
}

void UILoginWnd::OnCreate(int cx, int cy)
{
    if (!m_controlsCreated) {
        m_controlsCreated = true;
        Create(kPanelWidth, kPanelHeight);

        if (!IsQtUiRuntimeEnabled()) {
            for (const LoginButtonHitArea& spec : kLoginButtonAreas) {
                auto* button = new LoginButton();
                button->Create(spec.width, spec.height);
                button->m_id = spec.id;
                button->m_text = spec.label;
                AddChild(button);
            }
        }

        m_login = new LoginEditCtrl();
        m_login->Create(kFieldWidth, kFieldHeight);
        m_login->m_maxchar = 24;
        m_password = new LoginEditCtrl();
        m_password->Create(kFieldWidth, kFieldHeight);
        m_password->m_maxchar = 24;
        m_password->m_maskchar = '*';
        if (!IsQtUiRuntimeEnabled()) {
            AddChild(m_login);
            AddChild(m_password);
        }

        const std::string rememberedUserId = LoadSettingsIniString(
            kLoginSettingsSection, kRememberUserIdValue, "");
        m_saveAccountChecked = LoadSettingsIniInt(
            kLoginSettingsSection, kRememberUserIdEnabledValue, 0) != 0
            && !rememberedUserId.empty();
        if (m_saveAccountChecked) {
            m_login->SetText(rememberedUserId.c_str());
        }
        m_hasRememberedUserIdSnapshot = true;
        m_lastRememberedUserIdEnabled = m_saveAccountChecked;
        m_lastRememberedUserId = GetLoginText();
        SetLoginFieldFocus(m_login, m_password, GetLoginText()[0] != '\0');
    }

    Move((std::max)(0, (GetMenuLayoutClientExtent(cx) - m_w) / 2),
        (std::max)(0, (GetMenuLayoutClientExtent(cy) - m_h) / 2));
    LayoutControls();
}

void UILoginWnd::OnDraw()
{
    if (!g_hMainWnd || m_show == 0 || IsQtUiRuntimeEnabled()) {
        return;
    }
    EnsureQtLayout();
    HDC dc = AcquireDrawTarget();
    if (!dc) {
        return;
    }

    // The window manager draws the wallpaper; the login window draws only its UI.
    const RECT shadow{ m_x + 3, m_y + 3, m_x + m_w + 3, m_y + m_h + 3 };
    DrawSolidBox(dc, shadow, uitheme::kShadow, uitheme::kShadow);
    const RECT panel{ m_x, m_y, m_x + m_w, m_y + m_h };
    DrawSolidBox(dc, panel, uitheme::kPanelBackground, uitheme::kFrame);
    DrawLoginText(dc, { m_x + 24, m_y + 18, m_x + m_w - 24, m_y + 44 },
        "Login", uitheme::kUiText, 22, true);
    DrawLoginText(dc, { m_x + 24, m_y + 54, m_x + m_w - 24, m_y + 72 },
        "User", uitheme::kUiText);
    DrawLoginText(dc, { m_x + 24, m_y + 118, m_x + m_w - 24, m_y + 136 },
        "Password", uitheme::kUiText);
    DrawChildrenToHdc(dc);
    ReleaseDrawTarget(dc);
}

msgresult_t UILoginWnd::SendMsg(UIWindow* sender, int msg, msgparam_t wparam,
    msgparam_t lparam, msgparam_t extra)
{
    (void)lparam;
    (void)extra;
    const int buttonId = msg == 6 ? static_cast<int>(wparam) : sender ? sender->m_id : 0;
    switch (buttonId) {
    case 120:
        PlayUiButtonSound();
        RefreshRememberedUserIdStorage();
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetPassword,
            reinterpret_cast<msgparam_t>(m_password ? m_password->GetText() : ""), 0, 0);
        g_modeMgr.SendMsg(CLoginMode::LoginMsg_SetUserId,
            reinterpret_cast<msgparam_t>(GetLoginText()), 0, 0);
        return g_modeMgr.SendMsg(CLoginMode::LoginMsg_RequestConnect, 0, 0, 0);
    case 155:
        PlayUiButtonSound();
        return g_modeMgr.SendMsg(CLoginMode::LoginMsg_Quit, 0, 0, 0);
    default:
        return 0;
    }
}

void UILoginWnd::OnKeyDown(int virtualKey)
{
    if (virtualKey == VK_RETURN) {
        SendMsg(nullptr, 6, 120, 0, 0);
    } else if (virtualKey == VK_ESCAPE) {
        SendMsg(nullptr, 6, 155, 0, 0);
    } else if (virtualKey == VK_TAB && m_login && m_password) {
        SetLoginFieldFocus(m_login, m_password, !IsPasswordFocused());
    }
}
