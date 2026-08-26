#pragma once

#include <oleacc.h>

// Windows exposes SetHwndPropStr as an IAccPropServices method, not as a
// standalone function. Keep this small wrapper local to the native
// accessibility module so callers can annotate Win32 controls for NVDA.
inline HRESULT SetHwndPropStr(HWND hwnd, DWORD idObject, DWORD idChild,
                              MSAAPROPID idProp, LPCWSTR value)
{
    IAccPropServices* services = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_AccPropServices, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IAccPropServices,
                                  reinterpret_cast<void**>(&services));
    if (FAILED(hr) || !services)
        return hr;

    hr = services->SetHwndPropStr(hwnd, idObject, idChild, idProp, value);
    services->Release();
    return hr;
}

namespace NativeAccessibilityFeatures
{
    void Start();
    void Stop();
}