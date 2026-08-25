#pragma once

// Windows headers may define min/max as macros. Undefine them before the
// native UI implementation uses std::min/std::max.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace NativeWindowsUI
{
    // Installs a thread-local UI hook before the MFC modal loop starts.
    // All window changes are performed on the main Windows UI thread.
    void Start();
    void Stop();
}
