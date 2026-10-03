#include "Mode.h"
#include "LoginMode.h"
#include "GameMode.h"
#include "DebugLog.h"
#include "core/ClientInfoLocale.h"
#include "core/Timer.h"
#include "qtui/QtUiRuntime.h"
#include "render/Renderer.h"
#include "render3d/RenderDevice.h"
#include "render3d/GraphicsSettings.h"
#include "res/Texture.h"
#include "ui/UIWindowMgr.h"
#include "main/WinMain.h"
#include "world/GameActor.h"
#include <cstring>
#include <algorithm>
#include <array>
#include <chrono>
#include <windows.h>

namespace {

void RefreshFrameRate()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point nextRefresh{};
    static unsigned int activeFrameRate = 0;
    const auto now = Clock::now();
    if (now < nextRefresh) {
        return;
    }
    nextRefresh = now + std::chrono::seconds(1);

    const int requestedFrameRate = GetCachedGraphicsSettings().frameRate;
    unsigned int frameRate = requestedFrameRate > 0
        ? static_cast<unsigned int>(requestedFrameRate) : 60u;
#if RO_PLATFORM_WINDOWS
    if (requestedFrameRate == 0) {
        MONITORINFOEXW monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        DEVMODEW displayMode{};
        displayMode.dmSize = sizeof(displayMode);
        const HMONITOR monitor = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&monitorInfo))
            && EnumDisplaySettingsW(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &displayMode)
            && displayMode.dmDisplayFrequency > 1) {
            frameRate = (std::max)(24u, (std::min)(360u,
                static_cast<unsigned int>(displayMode.dmDisplayFrequency)));
        }
    }
#endif
    if (frameRate != activeFrameRate) {
        InitTimer(frameRate);
        activeFrameRate = frameRate;
        DbgLog("[Mode] Frame pacing target=%u FPS (%s), verticalSync=%d\n",
            frameRate, requestedFrameRate == 0 ? "automatic" : "configured",
            GetCachedGraphicsSettings().verticalSync ? 1 : 0);
    }
}

void RecordFramePacing()
{
    using Clock = std::chrono::steady_clock;
    static Clock::time_point previousFrame{};
    static Clock::time_point reportStart{};
    static std::array<double, 2048> intervals{};
    static size_t count = 0;
    const auto now = Clock::now();
    if (previousFrame == Clock::time_point{}) {
        previousFrame = now;
        reportStart = now;
        return;
    }
    const double intervalMs = std::chrono::duration<double, std::milli>(now - previousFrame).count();
    previousFrame = now;
    if (count < intervals.size()) {
        intervals[count++] = intervalMs;
    }
    if (now - reportStart < std::chrono::seconds(5)) {
        return;
    }
    double totalMs = 0.0;
    for (size_t i = 0; i < count; ++i) {
        totalMs += intervals[i];
    }
    std::sort(intervals.begin(), intervals.begin() + count);
    const double meanMs = totalMs / static_cast<double>(count);
    DbgLog("[FramePacing] frames=%zu fps=%.2f mean=%.3fms p95=%.3fms p99=%.3fms max=%.3fms\n",
        count, 1000.0 / meanMs, meanMs, intervals[(count - 1) * 95 / 100],
        intervals[(count - 1) * 99 / 100], intervals[count - 1]);
    count = 0;
    reportStart = now;
}

std::string ChooseRandomLoadingWallpaper()
{
    RefreshDefaultLoadingScreenList();
    const std::vector<std::string>& loadingScreens = GetLoadingScreenList();
    if (loadingScreens.empty()) {
        return {};
    }

    const DWORD tick = GetTickCount();
    const size_t index = static_cast<size_t>(tick % static_cast<DWORD>(loadingScreens.size()));
    return loadingScreens[index];
}

bool QueueFullScreenOverlayQuad(CTexture* texture, int width, int height, float sortKey)
{
    if (!texture || width <= 0 || height <= 0) {
        return false;
    }

    RPFace* face = g_renderer.BorrowNullRP();
    if (!face) {
        return false;
    }

    const float right = static_cast<float>(width) - 0.5f;
    const float bottom = static_cast<float>(height) - 0.5f;
    const unsigned int overlayContentWidth = texture->m_surfaceUpdateWidth > 0 ? texture->m_surfaceUpdateWidth : static_cast<unsigned int>(width);
    const unsigned int overlayContentHeight = texture->m_surfaceUpdateHeight > 0 ? texture->m_surfaceUpdateHeight : static_cast<unsigned int>(height);
    const float maxU = texture->m_w != 0 ? static_cast<float>(overlayContentWidth) / static_cast<float>(texture->m_w) : 1.0f;
    const float maxV = texture->m_h != 0 ? static_cast<float>(overlayContentHeight) / static_cast<float>(texture->m_h) : 1.0f;

    face->primType = D3DPT_TRIANGLESTRIP;
    face->verts = face->m_verts;
    face->numVerts = 4;
    face->indices = nullptr;
    face->numIndices = 0;
    face->tex = texture;
    face->mtPreset = 3;
    face->cullMode = D3DCULL_NONE;
    face->srcAlphaMode = D3DBLEND_SRCALPHA;
    face->destAlphaMode = D3DBLEND_INVSRCALPHA;
    face->alphaSortKey = sortKey;

    face->m_verts[0] = { -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, 0.0f };
    face->m_verts[1] = { right, -0.5f, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, 0.0f };
    face->m_verts[2] = { -0.5f, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, 0.0f, maxV };
    face->m_verts[3] = { right, bottom, 0.0f, 1.0f, 0xFFFFFFFFu, 0xFF000000u, maxU, maxV };
    g_renderer.AddRP(face, 1 | 8);
    return true;
}

bool QueueLoadingMenuOverlayQuad(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    static CTexture* s_qtLoadingOverlayTexture = nullptr;
    static int s_qtLoadingOverlayTextureWidth = 0;
    static int s_qtLoadingOverlayTextureHeight = 0;
    if (!s_qtLoadingOverlayTexture
        || s_qtLoadingOverlayTextureWidth != width
        || s_qtLoadingOverlayTextureHeight != height) {
        delete s_qtLoadingOverlayTexture;
        s_qtLoadingOverlayTexture = new CTexture();
        if (!s_qtLoadingOverlayTexture
            || !s_qtLoadingOverlayTexture->Create(width, height, PF_A8R8G8B8, false)) {
            delete s_qtLoadingOverlayTexture;
            s_qtLoadingOverlayTexture = nullptr;
            s_qtLoadingOverlayTextureWidth = 0;
            s_qtLoadingOverlayTextureHeight = 0;
            return false;
        }
        s_qtLoadingOverlayTextureWidth = width;
        s_qtLoadingOverlayTextureHeight = height;
    }

    if (!RenderQtUiMenuOverlayTexture(s_qtLoadingOverlayTexture, width, height)) {
        return false;
    }
    return QueueFullScreenOverlayQuad(s_qtLoadingOverlayTexture, width, height, 2.0f);
}

void DrawLoadingScreenFrame(const char* message, float progress)
{
    RefreshFrameRate();
    WaitForNextFrame();
    const std::string wallpaperName = ChooseRandomLoadingWallpaper();
    g_windowMgr.ShowLoadingScreen(wallpaperName, message ? message : "Loading...", progress);
    g_windowMgr.OnProcess();
    const bool hasLegacyDevice = GetRenderDevice().GetLegacyDevice() != nullptr;
    if (!hasLegacyDevice && IsQtUiRuntimeEnabled() && g_hMainWnd) {
        RECT clientRect{};
        GetClientRect(g_hMainWnd, &clientRect);
        const int clientWidth = clientRect.right - clientRect.left;
        const int clientHeight = clientRect.bottom - clientRect.top;
        if (clientWidth > 0 && clientHeight > 0) {
            g_renderer.ClearBackground();
            g_renderer.Clear(0);
            if (QueueLoadingMenuOverlayQuad(clientWidth, clientHeight)) {
                g_renderer.DrawScene();
                g_renderer.Flip();
            } else {
                g_windowMgr.OnDraw();
            }
        } else {
            g_windowMgr.OnDraw();
        }
    } else {
        g_windowMgr.OnDraw();
    }
    if (g_hMainWnd) {
        UpdateWindow(g_hMainWnd);
    }
}

CMode* CreateModeInstance(int modeType)
{
    switch (modeType) {
    case 0:
        return new CLoginMode();
    case 1:
        return new CGameMode();
    default:
        return new CLoginMode();
    }
}

} // namespace

CMode::CMode() 
    : m_subMode(0), m_subModeCnt(0), m_nextSubMode(-1), m_fadeInCount(0)
    , m_loopCond(1), m_isConnected(0), m_helpBalloon(nullptr), m_helpBalloonTick(0)
    , m_mouseAnimStartTick(0), m_isMouseLockOn(0), m_screenShotNow(0)
    , m_cursorActNum(0), m_cursorMotNum(0)
{
    m_mouseSnapDiff.x = 0;
    m_mouseSnapDiff.y = 0;
}

CMode::~CMode() = default;

void CMode::SetCursorAction(CursorAction cursorActNum)
{
    SetCursorAction(static_cast<int>(cursorActNum));
}

void CMode::SetCursorAction(int cursorActNum)
{
    if (m_cursorActNum == cursorActNum) {
        return;
    }

    m_cursorActNum = cursorActNum;
    m_cursorMotNum = 0;
    m_mouseAnimStartTick = GetTickCount();
    g_windowMgr.SendMsg(15, m_cursorActNum, 0);
}

CModeMgr g_modeMgr;

CModeMgr::CModeMgr() 
    : m_loopCond(0), m_curMode(nullptr), m_curModeType(0), m_nextModeType(0)
{
    std::memset(m_curModeName, 0, 40);
    std::memset(m_nextModeName, 0, 40);
}

CModeMgr::~CModeMgr() = default;

void CModeMgr::Quit()
{
    m_loopCond = 0;
    PostQuitMessage(0);
}

void CModeMgr::Switch(int newMode, const char* worldName)
{
    m_nextModeType = newMode;
    if (worldName) {
        std::strncpy(m_nextModeName, worldName, 39);
        m_nextModeName[39] = '\0';
    }
}

void CModeMgr::PresentLoadingScreen(const char* message, float progress)
{
    DrawLoadingScreenFrame(message, progress);
}

msgresult_t CModeMgr::SendMsg(int msg, msgparam_t wparam, msgparam_t lparam, msgparam_t extra)
{
    if (!m_curMode) {
        return 0;
    }
    return m_curMode->SendMsg(msg, wparam, lparam, extra);
}

CGameMode* CModeMgr::GetCurrentGameMode() const
{
    return m_curModeType == 1 ? dynamic_cast<CGameMode*>(m_curMode) : nullptr;
}

CLoginMode* CModeMgr::GetCurrentLoginMode() const
{
    return m_curModeType == 0 ? dynamic_cast<CLoginMode*>(m_curMode) : nullptr;
}

void CModeMgr::Run(int startMode, const char* worldName)
{
    m_loopCond = 1;
    m_curModeType = -1;
    m_nextModeType = startMode;
    if (worldName) {
        std::strncpy(m_nextModeName, worldName, 39);
        m_nextModeName[39] = '\0';
    }

    MSG msg{};
    while (m_loopCond)
    {
        RefreshFrameRate();
        WaitForNextFrame();
        RecordFramePacing();

        // Pump input after the wait so this frame sees the latest events.
        // Bound the work so continuous mouse/window messages cannot starve rendering.
        for (unsigned int messages = 0; m_loopCond && messages < 256
            && PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE); ++messages) {
            if (msg.message == WM_QUIT) {
                m_loopCond = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (!m_loopCond) {
            break;
        }
        {
            const bool reloadCurrentMode = m_curMode
                && m_curModeType == m_nextModeType
                && std::strncmp(m_curModeName, m_nextModeName, sizeof(m_curModeName)) != 0;
            if (!m_curMode || m_curModeType != m_nextModeType || reloadCurrentMode) {
                if (m_curMode) {
                    DrawLoadingScreenFrame("Loading new map...", 0.01f);
                }
                if (m_curMode) {
                    m_curMode->OnExit();
                    delete m_curMode;
                    m_curMode = nullptr;
                }

                m_curModeType = m_nextModeType;
                std::strncpy(m_curModeName, m_nextModeName, 39);
                m_curModeName[39] = '\0';

                m_curMode = CreateModeInstance(m_curModeType);
                if (m_curMode) {
                    m_curMode->OnInit(m_curModeName);
                }
            }

            if (m_curMode) {
                ProcessQtUiRuntimeEvents();
                if (m_loopCond) {
                    m_curMode->OnRun();
                    RecordMainWindowFrame();
                }
            }
        }
    }

    if (m_curMode) {
        DbgLog("[Mode] Final mode exit starting type=%d name='%s' ptr=%p\n",
            m_curModeType,
            m_curModeName,
            static_cast<void*>(m_curMode));
        m_curMode->OnExit();
        DbgLog("[Mode] Final mode exit completed type=%d name='%s' ptr=%p\n",
            m_curModeType,
            m_curModeName,
            static_cast<void*>(m_curMode));
        delete m_curMode;
        DbgLog("[Mode] Final mode delete completed type=%d name='%s'\n",
            m_curModeType,
            m_curModeName);
        m_curMode = nullptr;
    }
    ClearSharedActorBillboardCaches();
}
