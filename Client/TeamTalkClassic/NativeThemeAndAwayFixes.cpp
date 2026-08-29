#include "stdafx.h"
#include "gui/SessionTreeCtrl.h"
#include "resource.h"

#include <commctrl.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

extern TTInstance* ttInst;

namespace
{
    constexpr UINT_PTR THEME_SUBCLASS_ID = 0x54545044;

    constexpr COLORREF DARK_DIALOG_COLOR = RGB(32, 32, 32);
    constexpr COLORREF DARK_CONTROL_COLOR = RGB(45, 45, 48);
    constexpr COLORREF DARK_TEXT_COLOR = RGB(240, 240, 240);
    constexpr COLORREF AWAY_OVERLAY_COLOR = RGB(220, 0, 0);

    HHOOK g_postMessageHook = nullptr;
    HBRUSH g_darkDialogBrush = nullptr;
    HBRUSH g_darkControlBrush = nullptr;
    bool g_applyingTheme = false;

    using SetPreferredAppModeFn = int (WINAPI*)(int);
    using AllowDarkModeForWindowFn = bool (WINAPI*)(HWND, bool);
    using DwmSetWindowAttributeFn = HRESULT (WINAPI*)(HWND, DWORD, LPCVOID, DWORD);

    bool IsSystemDarkTheme()
    {
        DWORD appsUseLightTheme = 1;
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
        {
            DWORD type = 0;
            DWORD size = sizeof(appsUseLightTheme);
            if (RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(&appsUseLightTheme),
                                 &size) != ERROR_SUCCESS || type != REG_DWORD)
            {
                appsUseLightTheme = 1;
            }
            RegCloseKey(key);
        }
        return appsUseLightTheme == 0;
    }

    HBRUSH DarkDialogBrush()
    {
        if (!g_darkDialogBrush)
            g_darkDialogBrush = CreateSolidBrush(DARK_DIALOG_COLOR);
        return g_darkDialogBrush;
    }

    HBRUSH DarkControlBrush()
    {
        if (!g_darkControlBrush)
            g_darkControlBrush = CreateSolidBrush(DARK_CONTROL_COLOR);
        return g_darkControlBrush;
    }

    bool IsDialogClass(HWND hwnd)
    {
        wchar_t className[64] = {};
        GetClassNameW(hwnd, className, static_cast<int>(_countof(className)));
        return wcscmp(className, L"#32770") == 0;
    }

    void ConfigurePreferredAppMode(bool dark)
    {
        HMODULE theme = LoadLibraryW(L"uxtheme.dll");
        if (!theme)
            return;

        SetPreferredAppModeFn setPreferredAppMode =
            reinterpret_cast<SetPreferredAppModeFn>(
                GetProcAddress(theme, MAKEINTRESOURCEA(135)));
        if (setPreferredAppMode)
            setPreferredAppMode(dark ? 1 : 0); // AllowDark / Default

        FreeLibrary(theme);
    }

    void AllowDarkModeForWindow(HWND hwnd, bool dark)
    {
        HMODULE theme = LoadLibraryW(L"uxtheme.dll");
        if (!theme)
            return;

        AllowDarkModeForWindowFn allowDarkModeForWindow =
            reinterpret_cast<AllowDarkModeForWindowFn>(
                GetProcAddress(theme, MAKEINTRESOURCEA(133)));
        if (allowDarkModeForWindow)
            allowDarkModeForWindow(hwnd, dark);

        FreeLibrary(theme);
    }

    void ApplyDarkTitleBar(HWND hwnd, bool dark)
    {
        if (!hwnd)
            return;

        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        if (!dwm)
            return;

        DwmSetWindowAttributeFn setAttribute =
            reinterpret_cast<DwmSetWindowAttributeFn>(
                GetProcAddress(dwm, "DwmSetWindowAttribute"));
        if (setAttribute)
        {
            BOOL enabled = dark ? TRUE : FALSE;
            const DWORD immersiveDarkMode = 20;
            if (FAILED(setAttribute(hwnd, immersiveDarkMode,
                                    &enabled, sizeof(enabled))))
            {
                const DWORD immersiveDarkModeOld = 19;
                setAttribute(hwnd, immersiveDarkModeOld,
                             &enabled, sizeof(enabled));
            }
        }
        FreeLibrary(dwm);
    }

    void ApplyControlTheme(HWND hwnd, bool dark)
    {
        if (!hwnd)
            return;

        AllowDarkModeForWindow(hwnd, dark);
        SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

        wchar_t className[64] = {};
        GetClassNameW(hwnd, className, static_cast<int>(_countof(className)));

        if (wcscmp(className, L"SysTreeView32") == 0)
        {
            TreeView_SetBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            TreeView_SetTextColor(hwnd, dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT));
        }
        else if (wcscmp(className, L"SysListView32") == 0)
        {
            ListView_SetBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            ListView_SetTextBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            ListView_SetTextColor(hwnd, dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT));
        }

        InvalidateRect(hwnd, nullptr, TRUE);
    }

    LRESULT CALLBACK ThemeSubclassProc(HWND hwnd, UINT message,
                                       WPARAM wParam, LPARAM lParam,
                                       UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, ThemeSubclassProc, subclassId);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }

        if (IsSystemDarkTheme())
        {
            switch (message)
            {
            case WM_ERASEBKGND:
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                RECT client = {};
                GetClientRect(hwnd, &client);
                FillRect(dc, &client, DarkDialogBrush());
                return TRUE;
            }

            case WM_CTLCOLORDLG:
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, DARK_TEXT_COLOR);
                SetBkColor(dc, DARK_DIALOG_COLOR);
                return reinterpret_cast<LRESULT>(DarkDialogBrush());
            }

            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLORBTN:
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, DARK_TEXT_COLOR);
                SetBkColor(dc, DARK_DIALOG_COLOR);
                SetBkMode(dc, TRANSPARENT);
                return reinterpret_cast<LRESULT>(DarkDialogBrush());
            }

            case WM_CTLCOLOREDIT:
            case WM_CTLCOLORLISTBOX:
            case WM_CTLCOLORSCROLLBAR:
            {
                HDC dc = reinterpret_cast<HDC>(wParam);
                SetTextColor(dc, DARK_TEXT_COLOR);
                SetBkColor(dc, DARK_CONTROL_COLOR);
                return reinterpret_cast<LRESULT>(DarkControlBrush());
            }
            }
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    BOOL CALLBACK ApplyChildThemeProc(HWND child, LPARAM lParam)
    {
        const bool dark = lParam != 0;
        ApplyControlTheme(child, dark);
        if (IsDialogClass(child))
            SetWindowSubclass(child, ThemeSubclassProc, THEME_SUBCLASS_ID, 0);
        return TRUE;
    }

    void ApplyThemeToWindow(HWND hwnd)
    {
        if (!hwnd || g_applyingTheme)
            return;

        g_applyingTheme = true;
        const bool dark = IsSystemDarkTheme();

        ConfigurePreferredAppMode(dark);
        ApplyControlTheme(hwnd, dark);
        if (IsDialogClass(hwnd) || GetAncestor(hwnd, GA_ROOT) == hwnd)
            SetWindowSubclass(hwnd, ThemeSubclassProc, THEME_SUBCLASS_ID, 0);

        EnumChildWindows(hwnd, ApplyChildThemeProc, dark ? 1 : 0);

        HWND root = GetAncestor(hwnd, GA_ROOT);
        ApplyDarkTitleBar(root ? root : hwnd, dark);

        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
        g_applyingTheme = false;
    }

    void DrawAwayOverlays(HWND tree)
    {
        if (!tree || !ttInst || !IsWindowVisible(tree))
            return;

        HDC dc = GetDC(tree);
        if (!dc)
            return;

        HPEN pen = CreatePen(PS_SOLID, 2, AWAY_OVERLAY_COLOR);
        if (!pen)
        {
            ReleaseDC(tree, dc);
            return;
        }

        HGDIOBJ oldPen = SelectObject(dc, pen);
        const int oldBkMode = SetBkMode(dc, TRANSPARENT);

        HTREEITEM item = TreeView_GetFirstVisible(tree);
        while (item)
        {
            TVITEMW info = {};
            info.mask = TVIF_PARAM;
            info.hItem = item;
            if (TreeView_GetItem(tree, &info))
            {
                const DWORD_PTR data = static_cast<DWORD_PTR>(info.lParam);
                if ((data & TYPE_ITEMDATA) == USER_ITEMDATA)
                {
                    const int userId = static_cast<int>(data & ID_ITEMDATA);
                    User user = {};
                    if (TT_GetUser(ttInst, userId, &user) &&
                        (user.nStatusMode & STATUSMODE_MASK) == STATUSMODE_AWAY)
                    {
                        RECT textRect = {};
                        if (TreeView_GetItemRect(tree, item, &textRect, TRUE))
                        {
                            const int iconLeft = textRect.left - 19;
                            const int iconTop = textRect.top + 2;

                            // Paint last so the custom gender icon cannot cover the away marker.
                            const int left = iconLeft + 8;
                            const int top = iconTop + 1;
                            const int right = iconLeft + 15;
                            const int bottom = iconTop + 8;

                            MoveToEx(dc, left, top, nullptr);
                            LineTo(dc, right, bottom);
                            MoveToEx(dc, right, top, nullptr);
                            LineTo(dc, left, bottom);
                        }
                    }
                }
            }

            item = TreeView_GetNextVisible(tree, item);
        }

        SetBkMode(dc, oldBkMode);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        ReleaseDC(tree, dc);
    }

    LRESULT CALLBACK PostMessageHookProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && lParam)
        {
            const CWPRETSTRUCT* message = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
            if (message && !g_applyingTheme)
            {
                switch (message->message)
                {
                case WM_INITDIALOG:
                    ApplyThemeToWindow(message->hwnd);
                    break;

                case WM_CREATE:
                    ApplyControlTheme(message->hwnd, IsSystemDarkTheme());
                    if (IsDialogClass(message->hwnd))
                        SetWindowSubclass(message->hwnd, ThemeSubclassProc,
                                          THEME_SUBCLASS_ID, 0);
                    break;

                case WM_SETTINGCHANGE:
                case WM_THEMECHANGED:
                {
                    HWND root = GetAncestor(message->hwnd, GA_ROOT);
                    ApplyThemeToWindow(root ? root : message->hwnd);
                    break;
                }

                case WM_PAINT:
                    if (GetDlgCtrlID(message->hwnd) == IDC_TREE_SESSION)
                        DrawAwayOverlays(message->hwnd);
                    break;
                }
            }
        }

        return CallNextHookEx(g_postMessageHook, code, wParam, lParam);
    }

    struct ThemeAndAwayFixBootstrap
    {
        ThemeAndAwayFixBootstrap()
        {
            ConfigurePreferredAppMode(IsSystemDarkTheme());
            g_postMessageHook = SetWindowsHookExW(WH_CALLWNDPROCRET,
                                                  PostMessageHookProc,
                                                  nullptr,
                                                  GetCurrentThreadId());
        }

        ~ThemeAndAwayFixBootstrap()
        {
            if (g_postMessageHook)
            {
                UnhookWindowsHookEx(g_postMessageHook);
                g_postMessageHook = nullptr;
            }
            if (g_darkDialogBrush)
            {
                DeleteObject(g_darkDialogBrush);
                g_darkDialogBrush = nullptr;
            }
            if (g_darkControlBrush)
            {
                DeleteObject(g_darkControlBrush);
                g_darkControlBrush = nullptr;
            }
        }
    };

    ThemeAndAwayFixBootstrap g_themeAndAwayFixBootstrap;
}
