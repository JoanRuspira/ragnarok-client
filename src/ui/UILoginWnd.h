#pragma once

#include "UIFrameWnd.h"
#include <string>

class UILoginWnd : public UIFrameWnd {
public:
    struct QtButtonDisplay {
        int id = 0;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        const char* label = "";
    };

    UILoginWnd();
    ~UILoginWnd() override;

    void OnCreate(int cx, int cy) override;
    void OnDraw() override;
    msgresult_t SendMsg(UIWindow* sender, int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra) override;
    void OnKeyDown(int virtualKey) override;
    bool HandleQtMouseDown(int x, int y);
    void EnsureQtLayout();
    void RefreshRememberedUserIdStorage();
    const char* GetLoginText() const;
    int GetPasswordLength() const;
    bool IsSaveAccountChecked() const;
    bool IsPasswordFocused() const;
    int GetQtButtonCount() const;
    bool GetQtButtonDisplayForQt(int index, QtButtonDisplay* outButton) const;

private:
    void LayoutControls();

    bool m_controlsCreated;
    bool m_saveAccountChecked;
    UIEditCtrl* m_login;
    UIEditCtrl* m_password;
    bool m_hasRememberedUserIdSnapshot;
    bool m_lastRememberedUserIdEnabled;
    std::string m_lastRememberedUserId;
};
