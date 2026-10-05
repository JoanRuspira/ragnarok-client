#pragma once

#include "UIFrameWnd.h"
#include "UIShopCommon.h"
#include "render/DC.h"

#include <array>
#include <string>
#include <vector>

class QImage;

class UIRoMapWnd : public UIFrameWnd {
public:
    struct DisplayMarker {
        int x = 0;
        int y = 0;
        int radius = 0;
        unsigned int color = 0;
    };

    struct DisplayZoomButton {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int direction = 0;
        bool enabled = false;
        bool pressed = false;
    };

    struct DisplayData {
        int mapX = 0;
        int mapY = 0;
        int mapWidth = 0;
        int mapHeight = 0;
        bool playerVisible = false;
        int playerX = 0;
        int playerY = 0;
        int playerDirection = 0;
        int imageRevision = 0;
        std::string mapName;
        std::vector<DisplayMarker> markers;
        std::array<DisplayZoomButton, 2> zoomButtons{};
    };

    UIRoMapWnd();
    ~UIRoMapWnd() override;

    void SetShow(int show) override;
    bool IsUpdateNeed() override;
    void OnCreate(int x, int y) override;
    void OnProcess() override;
    void OnDraw() override;
    void OnLBtnDown(int x, int y) override;
    void OnLBtnDblClk(int x, int y) override;
    void OnLBtnUp(int x, int y) override;
    void OnWheel(int delta) override;
    void StoreInfo() override;
    void DrawToHdc(HDC hdc, int drawX, int drawY);
    bool HasLoadedMinimap() const;
    bool GetDisplayDataForQt(DisplayData* outData) const;
    bool BuildQtMinimapImage(QImage* outImage) const;
    bool BuildOverlayImageForRenderer(QImage* outImage) const;

private:
    void EnsureCreated();
    void ReleaseAssets();
    void DrawZoomButtons(HDC hdc, int drawX, int drawY);
    void EnsureRenderCache();
    void ReleaseRenderCache();
    void InvalidateRenderCache();
    void DrawWindowContents(HDC hdc, int baseX, int baseY);
    void UpdateMinimapBitmap();
    RECT GetZoomButtonRect(int direction) const;
    bool CanZoom(int direction) const;
    std::string GetCurrentMinimapBitmapName() const;
    unsigned long long BuildVisualStateToken() const;

    bool m_controlsCreated;
    ArgbDibSurface m_renderCacheSurface;
    bool m_renderCacheDirty;
    int m_mapBitmapWidth;
    int m_mapBitmapHeight;
    std::vector<u32> m_mapPixels;
    std::string m_loadedBitmapName;
    std::string m_loadedBitmapPath;
    unsigned long long m_lastVisualStateToken;
    bool m_hasVisualStateToken;
    int m_lastPlayerX;
    int m_lastPlayerY;
    int m_lastPlayerDir;
    u32 m_lastDynamicInvalidateTick;
    int m_pressedZoomDirection;
};
