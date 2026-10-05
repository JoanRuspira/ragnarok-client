#include "UiTheme.h"

#include "DebugLog.h"

#include <algorithm>

namespace uitheme {

namespace {

#if RO_PLATFORM_WINDOWS
void DrawRoundedRect(HDC hdc, const RECT& rect, HBRUSH brush, HPEN pen, int radius)
{
    if (!hdc || rect.right <= rect.left || rect.bottom <= rect.top) {
        return;
    }

    const int maxRadius = static_cast<int>((std::min)(rect.right - rect.left, rect.bottom - rect.top)) / 2;
    const int cornerRadius = std::clamp(radius, 0, maxRadius);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);
    if (cornerRadius > 0) {
        // GDI takes ellipse diameters; QML takes a corner radius.
        RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, cornerRadius * 2, cornerRadius * 2);
    } else {
        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    }
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
}
#endif

} // namespace

void FillRoundedRect(HDC hdc, const RECT& rect, COLORREF color, int radius)
{
#if RO_PLATFORM_WINDOWS
    HBRUSH brush = CreateSolidBrush(color);
    DrawRoundedRect(hdc, rect, brush, static_cast<HPEN>(GetStockObject(NULL_PEN)), radius);
    DeleteObject(brush);
#else
    (void)hdc;
    (void)rect;
    (void)color;
    (void)radius;
#endif
}

void FrameRoundedRect(HDC hdc, const RECT& rect, COLORREF color, int radius)
{
#if RO_PLATFORM_WINDOWS
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    DrawRoundedRect(hdc, rect, static_cast<HBRUSH>(GetStockObject(NULL_BRUSH)), pen, radius);
    DeleteObject(pen);
#else
    (void)hdc;
    (void)rect;
    (void)color;
    (void)radius;
#endif
}

void InitializeFonts()
{
#if RO_PLATFORM_WINDOWS
    // Register embedded fonts for this process without installing them on Windows.
    static const bool initialized = [] {
        const char* resourceNames[] = { "TRAJAN_PRO_REGULAR", "TRAJAN_PRO_BOLD" };
        const HMODULE module = GetModuleHandleA(nullptr);
        for (const char* name : resourceNames) {
            const HRSRC resource = FindResourceA(module, name, MAKEINTRESOURCEA(10));
            const HGLOBAL loaded = resource ? LoadResource(module, resource) : nullptr;
            void* data = loaded ? LockResource(loaded) : nullptr;
            const DWORD size = resource ? SizeofResource(module, resource) : 0;
            DWORD fontCount = 0;
            if (!data || size == 0 || !AddFontMemResourceEx(data, size, nullptr, &fontCount)) {
                DbgLog("[UI] Failed to register embedded font %s\n", name);
            }
        }
        return true;
    }();
    (void)initialized;
#endif
}

} // namespace uitheme
