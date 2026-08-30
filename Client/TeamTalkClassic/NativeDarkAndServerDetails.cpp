#include "stdafx.h"
#include "AppInfo.h"
#include "HttpRequest.h"
#include "resource.h"

#include <algorithm>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>

#include <commctrl.h>
#include <richedit.h>
#include <uxtheme.h>
#include <tinyxml2.h>

#pragma comment(lib, "uxtheme.lib")

namespace
{
    constexpr UINT WM_TT_APPLY_MODERN_UI = WM_APP + 0x341;
    constexpr UINT_PTR TIMER_NATIVE_SERVER_DETAILS = 0x6A41;
    constexpr UINT_PTR HEADING_SUBCLASS_ID = 0x54544631;

    constexpr int IDC_NATIVE_CHANNELS_HEADING = 60001;
    constexpr int IDC_NATIVE_CONTENT_HEADING = 60002;
    constexpr ULONG_PTR PUBSERVER_ITEMDATA = 0x80000000ULL;

    constexpr COLORREF DARK_DIALOG_COLOR = RGB(32, 32, 32);
    constexpr COLORREF DARK_CONTROL_COLOR = RGB(45, 45, 48);
    constexpr COLORREF DARK_TEXT_COLOR = RGB(240, 240, 240);

    const wchar_t* SERVER_DETAILS_PROP = L"TeamTalkPro.NativeServerDetailsV3";

    struct ServerMeta
    {
        std::wstring name;
        std::wstring serverName;
        std::wstring listing;
        std::wstring country;
        std::wstring motd;
        int userCount = 0;
    };

    struct ServerDetailsState
    {
        std::unique_ptr<CHttpRequest> request;
        std::vector<ServerMeta> servers;
        ULONGLONG started = 0;
        ULONGLONG parsedAt = 0;
    };

    HHOOK g_hook = nullptr;
    HBRUSH g_headingBrush = nullptr;

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

    HBRUSH HeadingBrush()
    {
        if (!g_headingBrush)
            g_headingBrush = CreateSolidBrush(DARK_DIALOG_COLOR);
        return g_headingBrush;
    }

    bool IsRichEdit(HWND hwnd)
    {
        if (!hwnd)
            return false;
        wchar_t className[64] = {};
        GetClassNameW(hwnd, className, static_cast<int>(_countof(className)));
        return _wcsnicmp(className, L"RichEdit", 8) == 0 ||
               _wcsnicmp(className, L"RICHEDIT", 8) == 0;
    }

    bool IsHostManager(HWND hwnd)
    {
        return hwnd &&
               GetDlgItem(hwnd, IDC_LIST_HOSTS) &&
               GetDlgItem(hwnd, IDC_CHECK_PUBLICSERVERS) &&
               GetDlgItem(hwnd, IDC_COMBO_HOSTADDRESS);
    }

    LRESULT CALLBACK HeadingProc(HWND hwnd, UINT message,
                                 WPARAM wParam, LPARAM lParam,
                                 UINT_PTR subclassId, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hwnd, HeadingProc, subclassId);
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

            RECT rc = {};
            GetClientRect(hwnd, &rc);
            FillRect(dc, &rc, HeadingBrush());

            wchar_t text[512] = {};
            GetWindowTextW(hwnd, text, static_cast<int>(_countof(text)));

            HFONT font = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
            HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
            SetTextColor(dc, DARK_TEXT_COLOR);
            SetBkMode(dc, TRANSPARENT);
            DrawTextW(dc, text, -1, &rc,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT |
                      DT_END_ELLIPSIS | DT_NOPREFIX);
            if (oldFont)
                SelectObject(dc, oldFont);

            EndPaint(hwnd, &ps);
            return 0;
        }

        return DefSubclassProc(hwnd, message, wParam, lParam);
    }

    void ApplyRichEditDark(HWND hwnd, bool dark)
    {
        if (!IsRichEdit(hwnd))
            return;

        SendMessageW(hwnd, EM_SETBKGNDCOLOR, 0,
                     dark ? DARK_CONTROL_COLOR : GetSysColor(COLOR_WINDOW));

        CHARFORMAT2W format = {};
        format.cbSize = sizeof(format);
        format.dwMask = CFM_COLOR;
        format.crTextColor = dark ? DARK_TEXT_COLOR : GetSysColor(COLOR_WINDOWTEXT);
        SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_ALL,
                     reinterpret_cast<LPARAM>(&format));
        SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_DEFAULT,
                     reinterpret_cast<LPARAM>(&format));
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    BOOL CALLBACK ApplyMissingDarkPiecesProc(HWND child, LPARAM lParam)
    {
        const bool dark = lParam != 0;
        const int id = GetDlgCtrlID(child);

        if (id == IDC_NATIVE_CHANNELS_HEADING || id == IDC_NATIVE_CONTENT_HEADING)
        {
            if (dark)
                SetWindowSubclass(child, HeadingProc, HEADING_SUBCLASS_ID, 0);
            else
                RemoveWindowSubclass(child, HeadingProc, HEADING_SUBCLASS_ID);
            InvalidateRect(child, nullptr, TRUE);
        }

        ApplyRichEditDark(child, dark);

        wchar_t className[64] = {};
        GetClassNameW(child, className, static_cast<int>(_countof(className)));
        if (wcscmp(className, L"SysTabControl32") == 0)
        {
            SetWindowTheme(child, dark ? L"DarkMode_Explorer" : L"Explorer", nullptr);
            InvalidateRect(child, nullptr, TRUE);
        }
        else if (wcscmp(className, L"SysHeader32") == 0)
        {
            SetWindowTheme(child, dark ? L"DarkMode_ItemsView" : L"Explorer", nullptr);
            InvalidateRect(child, nullptr, TRUE);
        }

        return TRUE;
    }

    void ApplyMissingDarkPieces(HWND hwnd)
    {
        if (!hwnd)
            return;

        HWND root = GetAncestor(hwnd, GA_ROOT);
        if (!root)
            root = hwnd;

        const bool dark = IsSystemDarkTheme();
        EnumChildWindows(root, ApplyMissingDarkPiecesProc, dark ? 1 : 0);
    }

    std::wstring Utf8ToWide(const char* text)
    {
        if (!text || !*text)
            return std::wstring();
        CString value = STR_UTF8(text);
        return std::wstring(value.GetString());
    }

    std::wstring Trim(const std::wstring& value)
    {
        size_t first = 0;
        while (first < value.size() && iswspace(value[first]))
            ++first;

        size_t last = value.size();
        while (last > first && iswspace(value[last - 1]))
            --last;

        return value.substr(first, last - first);
    }

    std::wstring NormalizeServerName(std::wstring value)
    {
        value = Trim(value);
        while (!value.empty() && (value.front() == L'^' || value.front() == L'~'))
        {
            value.erase(value.begin());
            value = Trim(value);
        }

        const std::wstring countryMarker = L", País: ";
        size_t marker = value.find(countryMarker);
        if (marker != std::wstring::npos)
            value.resize(marker);

        std::wstring compact;
        bool previousSpace = false;
        for (wchar_t ch : value)
        {
            if (iswspace(ch))
            {
                if (!previousSpace)
                    compact.push_back(L' ');
                previousSpace = true;
            }
            else
            {
                compact.push_back(static_cast<wchar_t>(towlower(ch)));
                previousSpace = false;
            }
        }
        return Trim(compact);
    }

    std::wstring ChildText(tinyxml2::XMLElement* parent, const char* childName)
    {
        if (!parent)
            return std::wstring();
        tinyxml2::XMLElement* child = parent->FirstChildElement(childName);
        return child ? Utf8ToWide(child->GetText()) : std::wstring();
    }

    void ParseServerMetadata(const CString& response, ServerDetailsState* state)
    {
        if (!state || response.IsEmpty())
            return;

        std::string xml = STR_UTF8(response, response.GetLength() * 4);
        tinyxml2::XMLDocument doc;
        if (doc.Parse(xml.c_str()) != tinyxml2::XML_SUCCESS)
            return;

        tinyxml2::XMLElement* root = doc.RootElement();
        if (!root)
            return;

        state->servers.clear();
        for (tinyxml2::XMLElement* host = root->FirstChildElement("host");
             host;
             host = host->NextSiblingElement("host"))
        {
            ServerMeta meta;
            meta.name = ChildText(host, "name");
            meta.listing = ChildText(host, "listing");

            tinyxml2::XMLElement* stats = host->FirstChildElement("stats");
            meta.country = ChildText(stats, "country");
            meta.motd = ChildText(stats, "motd");
            meta.serverName = ChildText(stats, "servername");

            std::wstring users = ChildText(stats, "user-count");
            if (!users.empty())
                meta.userCount = _wtoi(users.c_str());

            if (!meta.name.empty())
                state->servers.push_back(meta);
        }

        state->parsedAt = GetTickCount64();
    }

    std::wstring ListItemText(HWND list, int index)
    {
        int length = static_cast<int>(
            SendMessageW(list, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0));
        if (length == LB_ERR || length < 0)
            return std::wstring();

        std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1, L'\0');
        if (SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(index),
                         reinterpret_cast<LPARAM>(buffer.data())) == LB_ERR)
            return std::wstring();
        return std::wstring(buffer.data());
    }

    const ServerMeta* FindServerMeta(const ServerDetailsState* state,
                                     const std::wstring& rawName,
                                     bool official)
    {
        if (!state)
            return nullptr;

        const std::wstring normalized = NormalizeServerName(rawName);
        const ServerMeta* fallback = nullptr;

        for (const ServerMeta& meta : state->servers)
        {
            const bool isOfficial = _wcsicmp(meta.listing.c_str(), L"official") == 0;
            if (official != isOfficial)
                continue;

            if (_wcsicmp(meta.name.c_str(), rawName.c_str()) == 0)
                return &meta;

            if (!meta.serverName.empty() &&
                _wcsicmp(meta.serverName.c_str(), rawName.c_str()) == 0)
            {
                return &meta;
            }

            if (NormalizeServerName(meta.name) == normalized ||
                (!meta.serverName.empty() &&
                 NormalizeServerName(meta.serverName) == normalized))
            {
                return &meta;
            }

            if (!fallback && !normalized.empty())
            {
                const std::wstring normalizedMeta = NormalizeServerName(meta.name);
                if (!normalizedMeta.empty() &&
                    (normalizedMeta.find(normalized) != std::wstring::npos ||
                     normalized.find(normalizedMeta) != std::wstring::npos))
                {
                    fallback = &meta;
                }
            }
        }
        return fallback;
    }

    void DecorateServerList(HWND hwnd, const ServerDetailsState* state)
    {
        if (!state || state->servers.empty())
            return;

        HWND list = GetDlgItem(hwnd, IDC_LIST_HOSTS);
        if (!list)
            return;

        const int count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
        if (count == LB_ERR || count <= 0)
            return;

        const int selected = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        bool changed = false;
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);

        for (int i = 0; i < count; ++i)
        {
            const ULONG_PTR itemData = static_cast<ULONG_PTR>(
                SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(i), 0));
            if ((itemData & PUBSERVER_ITEMDATA) == 0)
                continue;

            std::wstring current = ListItemText(list, i);
            if (current.empty() ||
                current.rfind(L"Servidor oficial, ", 0) == 0 ||
                current.rfind(L"Servidor não oficial, ", 0) == 0)
            {
                continue;
            }

            const std::wstring officialPrefix = L"Official: ";
            const std::wstring unofficialPrefix = L"Unofficial: ";
            bool official = false;
            std::wstring rawName;

            if (current.rfind(officialPrefix, 0) == 0)
            {
                official = true;
                rawName = current.substr(officialPrefix.size());
            }
            else if (current.rfind(unofficialPrefix, 0) == 0)
            {
                rawName = current.substr(unofficialPrefix.size());
            }
            else
            {
                continue;
            }

            const std::wstring countryMarker = L", País: ";
            size_t countryPos = rawName.find(countryMarker);
            if (countryPos != std::wstring::npos)
                rawName.resize(countryPos);
            rawName = Trim(rawName);

            const ServerMeta* meta = FindServerMeta(state, rawName, official);
            if (!meta)
                continue;

            std::wstring displayName = Trim(meta->name);
            if (displayName.empty())
                displayName = rawName;

            std::wstring wanted = official
                ? L"Servidor oficial, Nome: "
                : L"Servidor não oficial, Nome: ";
            wanted += displayName;
            wanted += L", Usuários: ";
            wanted += std::to_wstring(meta->userCount);
            wanted += L", País: ";
            wanted += meta->country;
            wanted += L", Mensagem: ";
            wanted += meta->motd;

            SendMessageW(list, LB_DELETESTRING, static_cast<WPARAM>(i), 0);
            int inserted = static_cast<int>(
                SendMessageW(list, LB_INSERTSTRING, static_cast<WPARAM>(i),
                             reinterpret_cast<LPARAM>(wanted.c_str())));
            if (inserted != LB_ERR && inserted != LB_ERRSPACE)
            {
                SendMessageW(list, LB_SETITEMDATA, static_cast<WPARAM>(inserted),
                             static_cast<LPARAM>(itemData));
                changed = true;
            }
        }

        if (selected != LB_ERR)
            SendMessageW(list, LB_SETCURSEL, static_cast<WPARAM>(selected), 0);

        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        if (changed)
        {
            InvalidateRect(list, nullptr, TRUE);
            UpdateWindow(list);
        }
    }

    void StartServerDetails(HWND hwnd)
    {
        if (!hwnd || GetPropW(hwnd, SERVER_DETAILS_PROP))
            return;

        auto state = new ServerDetailsState();
        CString url = URL_PUBLICSERVER;
        url += _T("&official=1&unofficial=1");
        state->request.reset(new CHttpRequest(url));
        state->started = GetTickCount64();

        SetPropW(hwnd, SERVER_DETAILS_PROP, state);
        SetTimer(hwnd, TIMER_NATIVE_SERVER_DETAILS, 500, nullptr);
    }

    void PollServerDetails(HWND hwnd)
    {
        auto state = reinterpret_cast<ServerDetailsState*>(
            GetPropW(hwnd, SERVER_DETAILS_PROP));
        if (!state)
            return;

        if (state->request)
        {
            if (state->request->SendReady())
            {
                state->request->Send(_T("<") _T(TT_XML_ROOTNAME) _T("/>"));
            }
            else if (state->request->ResponseReady())
            {
                CString response = state->request->GetResponse();
                ParseServerMetadata(response, state);
                state->request.reset();
            }
            else if (GetTickCount64() - state->started > 12000)
            {
                state->request.reset();
            }
        }

        DecorateServerList(hwnd, state);

        if (!state->request && state->parsedAt &&
            GetTickCount64() - state->parsedAt > 15000)
        {
            KillTimer(hwnd, TIMER_NATIVE_SERVER_DETAILS);
        }
    }

    LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && lParam)
        {
            const CWPRETSTRUCT* message =
                reinterpret_cast<const CWPRETSTRUCT*>(lParam);
            if (message)
            {
                switch (message->message)
                {
                case WM_INITDIALOG:
                    ApplyMissingDarkPieces(message->hwnd);
                    if (IsHostManager(message->hwnd))
                        StartServerDetails(message->hwnd);
                    break;

                case WM_TT_APPLY_MODERN_UI:
                    ApplyMissingDarkPieces(message->hwnd);
                    break;

                case WM_SETTINGCHANGE:
                case WM_SYSCOLORCHANGE:
                    ApplyMissingDarkPieces(message->hwnd);
                    break;

                case WM_TIMER:
                    if (message->wParam == TIMER_NATIVE_SERVER_DETAILS &&
                        GetPropW(message->hwnd, SERVER_DETAILS_PROP))
                    {
                        PollServerDetails(message->hwnd);
                    }
                    break;

                case WM_NCDESTROY:
                    if (GetPropW(message->hwnd, SERVER_DETAILS_PROP))
                    {
                        KillTimer(message->hwnd, TIMER_NATIVE_SERVER_DETAILS);
                        auto state = reinterpret_cast<ServerDetailsState*>(
                            RemovePropW(message->hwnd, SERVER_DETAILS_PROP));
                        delete state;
                    }
                    break;
                }
            }
        }

        return CallNextHookEx(g_hook, code, wParam, lParam);
    }

    struct Bootstrap
    {
        Bootstrap()
        {
            g_hook = SetWindowsHookExW(WH_CALLWNDPROCRET, HookProc,
                                       nullptr, GetCurrentThreadId());
        }

        ~Bootstrap()
        {
            if (g_hook)
            {
                UnhookWindowsHookEx(g_hook);
                g_hook = nullptr;
            }
            if (g_headingBrush)
            {
                DeleteObject(g_headingBrush);
                g_headingBrush = nullptr;
            }
        }
    };

    Bootstrap g_bootstrap;
}
