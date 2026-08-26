#pragma once

#include <oleacc.h>

// Windows headers can expose max as a macro. NativeAccessibilityFeatures.cpp
// uses std::max while positioning the official female icon, so remove the
// legacy macro in this module before the C++ standard-library call is parsed.
#ifdef max
#undef max
#endif

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
