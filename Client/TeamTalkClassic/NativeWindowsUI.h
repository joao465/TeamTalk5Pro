#pragma once

namespace NativeWindowsUI
{
    // Installs a thread-local UI hook before the MFC modal loop starts.
    // All window changes are performed on the main Windows UI thread.
    void Start();
    void Stop();
}
