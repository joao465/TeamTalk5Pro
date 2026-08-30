#include "stdafx.h"
#include "gui/SessionTreeCtrl.h"
#include "resource.h"

#include <commctrl.h>
#include <richedit.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

extern TTInstance* ttInst;

namespace
{
    constexpr UINT WM_TT_APPLY_MODERN_UI = WM_APP + 0x341;

    constexpr UINT_PTR THEME_SUBCLASS_ID = 0x54545044;
    constexpr UINT_PTR TAB_THEME_SUBCLASS_ID = 0x54545045;
    constexpr UINT_PTR STATUS_THEME_SUBCLASS_ID = 0x54545046;
    constexpr UINT_PTR BORDER_THEME_SUBCLASS_ID = 0x54545047;
    constexpr UINT_PTR CHROME_THEME_SUBCLASS_ID = 0x54545048;

    constexpr int IDC_NATIVE_CHANNELS_HEADING = 60001;
    constexpr int IDC_NATIVE_CONTENT_HEADING = 60002;
    constexpr int IDC_NATIVE_VOLUME_LABEL = 60003;
    constexpr int IDC_NATIVE_MIC_LABEL = 60004;
    constexpr int IDC_NATIVE_VOICEACT_LABEL = 60005;
    constexpr int IDC_NATIVE_LEVEL_LABEL = 60006;

    constexpr COLORREF DARK_DIALOG_COLOR = RGB(32, 32, 32);
    constexpr COLORREF DARK_CONTROL_COLOR = RGB(45, 45, 48);
    constexpr COLORREF DARK_TEXT_COLOR = RGB(240, 240, 240);
    constexpr COLORREF DARK_BORDER_COLOR = RGB(78, 78, 82);
    constexpr COLORREF DARK_HOT_COLOR = RGB(62, 62, 66);
    constexpr COLORREF AWAY_OVERLAY_COLOR = RGB(220, 0, 0);

    HHOOK g_postMessageHook = nullptr;
    HBRUSH g_darkDialogBrush = nullptr;
    HBRUSH g_darkControlBrush = nullptr;
    bool g_applyingTheme = false;
    bool g_adjustingLayout = false;

    using SetPreferredAppModeFn = int (WINAPI*)(int);
    using AllowDarkModeForWindowFn = bool (WINAPI*)(HWND, bool);
    using RefreshImmersiveColorPolicyStateFn = void (WINAPI*)();
    using FlushMenuThemesFn = void (WINAPI*)();
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

    bool IsClass(HWND hwnd, const wchar_t* wanted)
    {
        if (!hwnd || !wanted)
            return false;
        wchar_t className[64] = {};
        GetClassNameW(hwnd, className, static_cast<int>(_countof(className)));
        return _wcsicmp(className, wanted) == 0;
    }

    bool IsDialogClass(HWND hwnd)
    {
        return IsClass(hwnd, L"#32770");
    }

    bool IsRichEditClass(const wchar_t* className)
    {
        if (!className || !*className)
            return false;
        return _wcsnicmp(className, L"RichEdit", 8) == 0 ||
               _wcsnicmp(className, L"RICHEDIT", 8) == 0;
    }

    bool IsMainWindow(HWND hwnd)
    {
        return hwnd && GetDlgItem(hwnd, IDC_TREE_SESSION) &&
               GetDlgItem(hwnd, IDC_TAB_CTRL) &&
               GetDlgItem(hwnd, IDC_NATIVE_CHANNELS_HEADING) &&
               GetDlgItem(hwnd, IDC_NATIVE_CONTENT_HEADING);
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
            setPreferredAppMode(dark ? 1 : 0);

        RefreshImmersiveColorPolicyStateFn refreshPolicy =
            reinterpret_cast<RefreshImmersiveColorPolicyStateFn>(
                GetProcAddress(theme, MAKEINTRESOURCEA(104)));
        if (refreshPolicy)
            refreshPolicy();

        FlushMenuThemesFn flushMenus =
            reinterpret_cast<FlushMenuThemesFn>(
                GetProcAddress(theme, MAKEINTRESOURCEA(136)));
        if (flushMenus)
            flushMenus();

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

    void DrawDarkBorder(HWND hwnd)
    {
        if (!hwnd || !IsSystemDarkTheme())
            return;

        HDC dc = GetWindowDC(hwnd);
        if (!dc)
            return;

        RECT windowRect = {};
        GetWindowRect(hwnd, &windowRect);
        const int width = windowRect.right - windowRect.left;
        const int height = windowRect.bottom - windowRect.top;

        HPEN pen = CreatePen(PS_SOLID, 1, DARK_BORDER_COLOR);
        HGDIOBJ oldPen = pen ? SelectObject(dc, pen) : nullptr;
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

        if (pen)
        {
            Rectangle(dc, 0, 0, width, height);
            if (width > 3 && height > 3)
                Rectangle(dc, 1, 1, width - 1, height - 1);
        }

        if (oldBrush)
            SelectObject(dc, oldBrush);
        if (oldPen)
            SelectObject(dc, oldPen);
        if (pen)
            DeleteObject(pen);
        ReleaseDC(hwnd, dc);
    }

    LRESULT CALLBACK BorderThemeProc(HWND hwnd, UINT message,
                                     WPARAM wParam, LPARAM lParam,
                                     UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, BorderThemeProc, subclassId);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }

        LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);

        if (IsSystemDarkTheme() &&
            (message == WM_NCPAINT || message == WM_NCACTIVATE ||
             message == WM_SETFOCUS || message == WM_KILLFOCUS))
        {
            DrawDarkBorder(hwnd);
        }

        return result;
    }

    void PaintStatusBar(HWND hwnd, HDC dc)
    {
        RECT client = {};
        GetClientRect(hwnd, &client);
        FillRect(dc, &client, DarkDialogBrush());

        HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
        HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
        SetTextColor(dc, DARK_TEXT_COLOR);
        SetBkMode(dc, TRANSPARENT);

        HPEN pen = CreatePen(PS_SOLID, 1, DARK_BORDER_COLOR);
        HGDIOBJ oldPen = pen ? SelectObject(dc, pen) : nullptr;

        int partCount = static_cast<int>(SendMessageW(hwnd, SB_GETPARTS, 0, 0));
        if (partCount <= 0)
            partCount = 1;

        for (int i = 0; i < partCount; ++i)
        {
            RECT part = client;
            if (partCount > 1)
                SendMessageW(hwnd, SB_GETRECT, static_cast<WPARAM>(i),
                             reinterpret_cast<LPARAM>(&part));

            wchar_t text[2048] = {};
            LRESULT textInfo = SendMessageW(hwnd, SB_GETTEXTW,
                                            static_cast<WPARAM>(i),
                                            reinterpret_cast<LPARAM>(text));
            const UINT type = HIWORD(textInfo);
            if ((type & SBT_OWNERDRAW) == 0 && text[0])
            {
                RECT textRect = part;
                textRect.left += 5;
                textRect.right -= 4;
                DrawTextW(dc, text, -1, &textRect,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                          DT_END_ELLIPSIS | DT_NOPREFIX);
            }

            if (pen && i + 1 < partCount)
            {
                MoveToEx(dc, part.right - 1, part.top + 2, nullptr);
                LineTo(dc, part.right - 1, part.bottom - 2);
            }
        }

        if (pen)
        {
            MoveToEx(dc, client.left, client.top, nullptr);
            LineTo(dc, client.right, client.top);
        }

        if (oldPen)
            SelectObject(dc, oldPen);
        if (pen)
            DeleteObject(pen);
        if (oldFont)
            SelectObject(dc, oldFont);
    }

    LRESULT CALLBACK StatusThemeProc(HWND hwnd, UINT message,
                                     WPARAM wParam, LPARAM lParam,
                                     UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, StatusThemeProc, subclassId);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }

        if (!IsSystemDarkTheme())
            return DefSubclassProc(hwnd, message, wParam, lParam);

        if (message == WM_ERASEBKGND)
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, DarkDialogBrush());
            return TRUE;
        }

        if (message == WM_PAINT)
        {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(hwnd, &ps);
            if (dc)
                PaintStatusBar(hwnd, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }

        if (message == WM_PRINTCLIENT && wParam)
        {
            PaintStatusBar(hwnd, reinterpret_cast<HDC>(wParam));
            return 0;
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    LRESULT CALLBACK ChromeBackgroundProc(HWND hwnd, UINT message,
                                          WPARAM wParam, LPARAM lParam,
                                          UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, ChromeBackgroundProc, subclassId);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }

        if (IsSystemDarkTheme() && message == WM_ERASEBKGND)
        {
            RECT client = {};
            GetClientRect(hwnd, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, DarkDialogBrush());
            return TRUE;
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    LRESULT CALLBACK TabThemeProc(HWND hwnd, UINT message,
                                  WPARAM wParam, LPARAM lParam,
                                  UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, TabThemeProc, subclassId);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }

        if (!IsSystemDarkTheme())
            return DefSubclassProc(hwnd, message, wParam, lParam);

        if (message == WM_ERASEBKGND)
            return TRUE;

        if (message == WM_PAINT)
        {
            PAINTSTRUCT ps = {};
            HDC dc = BeginPaint(hwnd, &ps);
            if (!dc)
                return 0;

            RECT client = {};
            GetClientRect(hwnd, &client);
            FillRect(dc, &client, DarkDialogBrush());

            HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, DARK_TEXT_COLOR);

            HPEN borderPen = CreatePen(PS_SOLID, 1, DARK_BORDER_COLOR);
            HGDIOBJ oldPen = borderPen ? SelectObject(dc, borderPen) : nullptr;
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));

            const int selected = TabCtrl_GetCurSel(hwnd);
            const int count = TabCtrl_GetItemCount(hwnd);
            for (int i = 0; i < count; ++i)
            {
                RECT tabRect = {};
                if (!TabCtrl_GetItemRect(hwnd, i, &tabRect))
                    continue;

                FillRect(dc, &tabRect,
                         i == selected ? DarkControlBrush() : DarkDialogBrush());
                if (borderPen)
                    Rectangle(dc, tabRect.left, tabRect.top,
                              tabRect.right, tabRect.bottom);

                wchar_t text[256] = {};
                TCITEMW item = {};
                item.mask = TCIF_TEXT;
                item.pszText = text;
                item.cchTextMax = static_cast<int>(_countof(text));
                if (TabCtrl_GetItem(hwnd, i, &item))
                {
                    RECT textRect = tabRect;
                    InflateRect(&textRect, -6, -2);
                    DrawTextW(dc, text, -1, &textRect,
                              DT_SINGLELINE | DT_VCENTER | DT_CENTER |
                              DT_END_ELLIPSIS | DT_NOPREFIX);
                }
            }

            if (GetFocus() == hwnd && selected >= 0)
            {
                RECT focusRect = {};
                if (TabCtrl_GetItemRect(hwnd, selected, &focusRect))
                {
                    InflateRect(&focusRect, -4, -3);
                    DrawFocusRect(dc, &focusRect);
                }
            }

            if (oldBrush)
                SelectObject(dc, oldBrush);
            if (oldPen)
                SelectObject(dc, oldPen);
            if (borderPen)
                DeleteObject(borderPen);
            if (oldFont)
                SelectObject(dc, oldFont);

            EndPaint(hwnd, &ps);
            return 0;
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    bool NeedsDarkBorder(const wchar_t* className)
    {
        return className &&
               (_wcsicmp(className, L"Edit") == 0 ||
                _wcsicmp(className, L"SysTreeView32") == 0 ||
                _wcsicmp(className, L"SysListView32") == 0 ||
                _wcsicmp(className, L"ListBox") == 0 ||
                _wcsicmp(className, L"msctls_progress32") == 0 ||
                IsRichEditClass(className));
    }

    void ApplyControlTheme(HWND hwnd, bool dark)
    {
        if (!hwnd)
            return;

        AllowDarkModeForWindow(hwnd, dark);

        wchar_t className[64] = {};
        GetClassNameW(hwnd, className, static_cast<int>(_countof(className)));

        if (_wcsicmp(className, L"SysTreeView32") == 0)
        {
            SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            TreeView_SetBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            TreeView_SetTextColor(hwnd, dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT));
        }
        else if (_wcsicmp(className, L"SysListView32") == 0)
        {
            SetWindowTheme(hwnd, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
            ListView_SetBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            ListView_SetTextBkColor(hwnd, dark ? DARK_DIALOG_COLOR : GetSysColor(COLOR_WINDOW));
            ListView_SetTextColor(hwnd, dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT));
        }
        else if (_wcsicmp(className, L"SysHeader32") == 0)
        {
            SetWindowTheme(hwnd, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
        }
        else if (_wcsicmp(className, L"SysTabControl32") == 0)
        {
            if (dark)
            {
                SetWindowTheme(hwnd, L"", nullptr);
                SetWindowSubclass(hwnd, TabThemeProc, TAB_THEME_SUBCLASS_ID, 0);
            }
            else
            {
                RemoveWindowSubclass(hwnd, TabThemeProc, TAB_THEME_SUBCLASS_ID);
                SetWindowTheme(hwnd, L"Explorer", nullptr);
            }
        }
        else if (_wcsicmp(className, L"msctls_statusbar32") == 0)
        {
            if (dark)
            {
                SetWindowTheme(hwnd, L"", nullptr);
                SendMessageW(hwnd, SB_SETBKCOLOR, 0,
                             static_cast<LPARAM>(DARK_DIALOG_COLOR));
                SetWindowSubclass(hwnd, StatusThemeProc,
                                  STATUS_THEME_SUBCLASS_ID, 0);
            }
            else
            {
                RemoveWindowSubclass(hwnd, StatusThemeProc,
                                     STATUS_THEME_SUBCLASS_ID);
                SetWindowTheme(hwnd, L"Explorer", nullptr);
                SendMessageW(hwnd, SB_SETBKCOLOR, 0,
                             static_cast<LPARAM>(CLR_DEFAULT));
            }
        }
        else if (_wcsicmp(className, L"ReBarWindow32") == 0)
        {
            SetWindowTheme(hwnd, dark ? L"" : L"Explorer", nullptr);
            if (dark)
            {
                SendMessageW(hwnd, RB_SETBKCOLOR, 0,
                             static_cast<LPARAM>(DARK_DIALOG_COLOR));
                SetWindowSubclass(hwnd, ChromeBackgroundProc,
                                  CHROME_THEME_SUBCLASS_ID, 0);
            }
            else
            {
                SendMessageW(hwnd, RB_SETBKCOLOR, 0,
                             static_cast<LPARAM>(CLR_DEFAULT));
                RemoveWindowSubclass(hwnd, ChromeBackgroundProc,
                                     CHROME_THEME_SUBCLASS_ID);
            }
        }
        else if (_wcsicmp(className, L"ToolbarWindow32") == 0)
        {
            SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            if (dark)
                SetWindowSubclass(hwnd, ChromeBackgroundProc,
                                  CHROME_THEME_SUBCLASS_ID, 0);
            else
                RemoveWindowSubclass(hwnd, ChromeBackgroundProc,
                                     CHROME_THEME_SUBCLASS_ID);
        }
        else
        {
            SetWindowTheme(hwnd, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }

        if (IsRichEditClass(className))
        {
            SendMessageW(hwnd, EM_SETBKGNDCOLOR, 0,
                         dark ? DARK_CONTROL_COLOR : GetSysColor(COLOR_WINDOW));
            CHARFORMAT2W format = {};
            format.cbSize = sizeof(format);
            format.dwMask = CFM_COLOR;
            format.crTextColor = dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT);
            SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_DEFAULT,
                         reinterpret_cast<LPARAM>(&format));
        }

        if (NeedsDarkBorder(className))
        {
            if (dark)
                SetWindowSubclass(hwnd, BorderThemeProc,
                                  BORDER_THEME_SUBCLASS_ID, 0);
            else
                RemoveWindowSubclass(hwnd, BorderThemeProc,
                                     BORDER_THEME_SUBCLASS_ID);
        }

        InvalidateRect(hwnd, nullptr, TRUE);
    }

    LRESULT HandleToolbarCustomDraw(LPARAM lParam)
    {
        LPNMHDR header = reinterpret_cast<LPNMHDR>(lParam);
        if (!header || header->code != NM_CUSTOMDRAW ||
            !IsClass(header->hwndFrom, L"ToolbarWindow32"))
        {
            return -1;
        }

        LPNMTBCUSTOMDRAW draw = reinterpret_cast<LPNMTBCUSTOMDRAW>(lParam);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
        {
            FillRect(draw->nmcd.hdc, &draw->nmcd.rc, DarkDialogBrush());
            return CDRF_NOTIFYITEMDRAW;
        }

        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
        {
            draw->clrText = DARK_TEXT_COLOR;
            draw->clrMark = DARK_TEXT_COLOR;
            draw->clrTextHighlight = DARK_TEXT_COLOR;
            draw->clrBtnFace = DARK_DIALOG_COLOR;
            draw->clrBtnHighlight = DARK_CONTROL_COLOR;
            draw->clrHighlightHotTrack = DARK_HOT_COLOR;
            draw->nStringBkMode = TRANSPARENT;
            draw->nHLStringBkMode = TRANSPARENT;
#ifdef TBCDRF_USECDCOLORS
            return TBCDRF_USECDCOLORS;
#else
            return CDRF_DODEFAULT;
#endif
        }

        return CDRF_DODEFAULT;
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

            case WM_NOTIFY:
            {
                LRESULT custom = HandleToolbarCustomDraw(lParam);
                if (custom != -1)
                    return custom;
                break;
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

        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (!root)
            root = hwnd;

        AllowDarkModeForWindow(root, dark);
        SetWindowTheme(root, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);

        if (IsDialogClass(hwnd) || root == hwnd)
            SetWindowSubclass(hwnd, ThemeSubclassProc, THEME_SUBCLASS_ID, 0);
        if (root != hwnd)
            SetWindowSubclass(root, ThemeSubclassProc, THEME_SUBCLASS_ID, 0);

        EnumChildWindows(root, ApplyChildThemeProc, dark ? 1 : 0);

        ApplyDarkTitleBar(root, dark);

        if (GetMenu(root))
            DrawMenuBar(root);

        RedrawWindow(root, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_FRAME |
                     RDW_ALLCHILDREN | RDW_UPDATENOW);

        g_applyingTheme = false;
    }

    bool IsNativeWorkspaceControl(int id)
    {
        switch (id)
        {
        case IDC_TREE_SESSION:
        case IDC_TAB_CTRL:
        case IDC_NATIVE_CHANNELS_HEADING:
        case IDC_NATIVE_CONTENT_HEADING:
        case IDC_NATIVE_VOLUME_LABEL:
        case IDC_NATIVE_MIC_LABEL:
        case IDC_NATIVE_VOICEACT_LABEL:
        case IDC_NATIVE_LEVEL_LABEL:
        case IDC_SLIDER_VOLUME:
        case IDC_SLIDER_GAINLEVEL:
        case IDC_SLIDER_VOICEACT:
        case IDC_PROGRESS_VOICEACT:
        case IDC_STATIC_VOLUME:
        case IDC_STATIC_MIKE:
        case IDC_STATIC_VOICEACT:
        case IDC_STATIC_VU:
        case IDC_STATIC_SPLITTER:
            return true;
        default:
            return false;
        }
    }

    bool ChildRectInParent(HWND child, HWND parent, RECT& rect)
    {
        if (!child || !parent || !GetWindowRect(child, &rect))
            return false;
        MapWindowPoints(nullptr, parent, reinterpret_cast<POINT*>(&rect), 2);
        return true;
    }

    int FindTopChromeBottom(HWND hwnd)
    {
        RECT client = {};
        GetClientRect(hwnd, &client);
        int best = 0;
        int fallback = 0;

        for (HWND child = GetWindow(hwnd, GW_CHILD); child;
             child = GetWindow(child, GW_HWNDNEXT))
        {
            if (!IsWindowVisible(child) || GetParent(child) != hwnd)
                continue;

            const int id = GetDlgCtrlID(child);
            if (IsNativeWorkspaceControl(id))
                continue;

            RECT rect = {};
            if (!ChildRectInParent(child, hwnd, rect))
                continue;

            const int childHeight = rect.bottom - rect.top;
            if (rect.bottom <= 0 || rect.top >= 90 || childHeight <= 0)
                continue;

            wchar_t className[64] = {};
            GetClassNameW(child, className, static_cast<int>(_countof(className)));
            if (_wcsicmp(className, L"ToolbarWindow32") == 0 ||
                _wcsicmp(className, L"ReBarWindow32") == 0)
            {
                best = max(best, static_cast<int>(rect.bottom));
                continue;
            }

            if (rect.top <= 45 && rect.bottom <= 80 && childHeight <= 45 &&
                rect.right > client.left && rect.left < client.right)
            {
                fallback = max(fallback, static_cast<int>(rect.bottom));
            }
        }

        return best > 0 ? best : fallback;
    }

    int FindStatusBarTop(HWND hwnd, int clientHeight)
    {
        int top = clientHeight;
        for (HWND child = GetWindow(hwnd, GW_CHILD); child;
             child = GetWindow(child, GW_HWNDNEXT))
        {
            if (!IsWindowVisible(child) || GetParent(child) != hwnd)
                continue;

            if (!IsClass(child, L"msctls_statusbar32"))
                continue;

            RECT rect = {};
            if (ChildRectInParent(child, hwnd, rect) &&
                rect.top > clientHeight / 2)
            {
                top = min(top, static_cast<int>(rect.top));
            }
        }
        return top;
    }

    void MoveNativeControl(HWND parent, int id,
                           int x, int y, int width, int height)
    {
        HWND control = GetDlgItem(parent, id);
        if (!control)
            return;
        MoveWindow(control, x, y, max(1, width), max(1, height), TRUE);
    }

    void FixMainLayout(HWND hwnd)
    {
        if (!IsMainWindow(hwnd) || g_adjustingLayout)
            return;

        RECT client = {};
        GetClientRect(hwnd, &client);
        const int width = client.right - client.left;
        const int height = client.bottom - client.top;
        if (width < 400 || height < 280)
            return;

        g_adjustingLayout = true;

        const int margin = 14;
        const int gap = 14;
        const int headingHeight = 24;
        const int bottomHeight = 86;

        const int chromeBottom = FindTopChromeBottom(hwnd);
        const int workspaceTop = chromeBottom > 0
            ? max(margin, chromeBottom + 7)
            : margin;

        const int statusTop = FindStatusBarTop(hwnd, height);
        const int usableBottom = min(height, statusTop);
        const int contentTop = workspaceTop + headingHeight;
        const int contentBottom = usableBottom - bottomHeight - margin;
        const int contentHeight = max(80, contentBottom - contentTop);

        const int usableWidth = width - margin * 2 - gap;
        int leftWidth = static_cast<int>(usableWidth * 0.36);
        leftWidth = max(230, min(leftWidth, usableWidth - 300));
        const int rightX = margin + leftWidth + gap;
        const int rightWidth = width - rightX - margin;

        MoveNativeControl(hwnd, IDC_NATIVE_CHANNELS_HEADING,
                          margin, workspaceTop, leftWidth, headingHeight);
        MoveNativeControl(hwnd, IDC_NATIVE_CONTENT_HEADING,
                          rightX, workspaceTop, rightWidth, headingHeight);
        MoveNativeControl(hwnd, IDC_TREE_SESSION,
                          margin, contentTop, leftWidth, contentHeight);
        MoveNativeControl(hwnd, IDC_TAB_CTRL,
                          rightX, contentTop, rightWidth, contentHeight);

        const int stripY = usableBottom - bottomHeight + 8;
        const int stripWidth = width - margin * 2;
        const int colGap = 12;
        const int colWidth = max(1, (stripWidth - colGap * 3) / 4);
        const int labelHeight = 20;
        const int controlHeight = 28;

        const int x1 = margin;
        const int x2 = x1 + colWidth + colGap;
        const int x3 = x2 + colWidth + colGap;
        const int x4 = x3 + colWidth + colGap;

        MoveNativeControl(hwnd, IDC_NATIVE_VOLUME_LABEL,
                          x1, stripY, colWidth, labelHeight);
        MoveNativeControl(hwnd, IDC_SLIDER_VOLUME,
                          x1, stripY + labelHeight, colWidth, controlHeight);
        MoveNativeControl(hwnd, IDC_NATIVE_MIC_LABEL,
                          x2, stripY, colWidth, labelHeight);
        MoveNativeControl(hwnd, IDC_SLIDER_GAINLEVEL,
                          x2, stripY + labelHeight, colWidth, controlHeight);
        MoveNativeControl(hwnd, IDC_NATIVE_VOICEACT_LABEL,
                          x3, stripY, colWidth, labelHeight);
        MoveNativeControl(hwnd, IDC_SLIDER_VOICEACT,
                          x3, stripY + labelHeight, colWidth, controlHeight);
        MoveNativeControl(hwnd, IDC_NATIVE_LEVEL_LABEL,
                          x4, stripY, colWidth, labelHeight);
        MoveNativeControl(hwnd, IDC_PROGRESS_VOICEACT,
                          x4, stripY + labelHeight + 6, colWidth, 16);

        RedrawWindow(hwnd, nullptr, nullptr,
                     RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
        g_adjustingLayout = false;
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
            const CWPRETSTRUCT* message =
                reinterpret_cast<const CWPRETSTRUCT*>(lParam);
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
                    {
                        SetWindowSubclass(message->hwnd, ThemeSubclassProc,
                                          THEME_SUBCLASS_ID, 0);
                    }
                    break;

                case WM_TT_APPLY_MODERN_UI:
                {
                    HWND root = GetAncestor(message->hwnd, GA_ROOT);
                    if (!root)
                        root = message->hwnd;
                    FixMainLayout(root);
                    ApplyThemeToWindow(root);
                    break;
                }

                case WM_SIZE:
                    if (IsMainWindow(message->hwnd))
                        FixMainLayout(message->hwnd);
                    break;

                case WM_SETTINGCHANGE:
                case WM_THEMECHANGED:
                case WM_SYSCOLORCHANGE:
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
