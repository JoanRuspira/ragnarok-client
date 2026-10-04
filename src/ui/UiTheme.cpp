#include "UiTheme.h"

#include "DebugLog.h"

namespace uitheme {

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
