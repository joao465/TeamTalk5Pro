#pragma once

namespace NativeParityFeatures
{
    // Installs the native parity hook on the current MFC UI thread.
    void Start();
    void Stop();

    // Mirrors the Qt display/show-unofficial-servers preference for the
    // native Host Manager without introducing a Qt dependency.
    bool ShowUnofficialServers();
}
