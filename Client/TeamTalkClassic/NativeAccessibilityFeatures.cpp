#include "stdafx.h"
#include "NativeAccessibilityFeatures.h"
#include "AppInfo.h"
#include "HttpRequest.h"
#include "TeamTalkBase.h"
#include "gui/SessionTreeCtrl.h"
#include "resource.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <commctrl.h>
#include <oleacc.h>
#include <tinyxml2.h>

#pragma comment(lib, "oleacc.lib")

extern TTInstance* ttInst;

namespace
{
    constexpr UINT_PTR TIMER_SOUND_ACCESSIBILITY = 0x6A31;
    constexpr UINT_PTR TIMER_SERVER_COUNTRY = 0x6A32;

    constexpr int IDC_NATIVE_SECONDARY_LABEL = 60101;
    constexpr int IDC_NATIVE_SECONDARY_COMBO = 60102;
    constexpr int IDC_NATIVE_EQ_BASS_LABEL = 60104;
    constexpr int IDC_NATIVE_EQ_BASS = 60105;
    constexpr int IDC_NATIVE_EQ_MID_LABEL = 60106;
    constexpr int IDC_NATIVE_EQ_MID = 60107;
    constexpr int IDC_NATIVE_EQ_TREBLE_LABEL = 60108;
    constexpr int IDC_NATIVE_EQ_TREBLE = 60109;

    constexpr ULONG_PTR PUBSERVER_ITEMDATA = 0x80000000ULL;

    const wchar_t* COUNTRY_STATE_PROP = L"TeamTalkPro.NativeCountryState";
    const wchar_t* TREE_STATE_PROP = L"TeamTalkPro.NativeUserTreeState";

    struct CountryState
    {
        std::unique_ptr<CHttpRequest> request;
        std::map<std::wstring, std::wstring> official;
        std::map<std::wstring, std::wstring> unofficial;
        ULONGLONG started = 0;
    };

    struct TreeState
    {
        WNDPROC previous = nullptr;
    };

    HHOOK g_hook = nullptr;
    bool g_decoratingTree = false;

    bool StartsWith(const std::wstring& text, const std::wstring& prefix)
    {
        return text.size() >= prefix.size() &&
               text.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(const std::wstring& text, const std::wstring& suffix)
    {
        return text.size() >= suffix.size() &&
               text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    void StripPrefix(std::wstring& text, const std::wstring& prefix)
    {
        if (StartsWith(text, prefix))
            text.erase(0, prefix.size());
    }

    void StripSuffix(std::wstring& text, const std::wstring& suffix)
    {
        if (EndsWith(text, suffix))
            text.erase(text.size() - suffix.size());
    }

    void SetAccessibleName(HWND control, const wchar_t* name, bool setWindowText)
    {
        if (!control)
            return;

        // MSAA dynamic annotation gives NVDA a stable name without changing
        // the selected value of combo boxes.
        SetHwndPropStr(control, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, name);
        if (setWindowText)
            SetWindowTextW(control, name);
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, control, OBJID_CLIENT, CHILDID_SELF);
    }

    bool ApplySoundAccessibility(HWND hwnd)
    {
        HWND secondary = GetDlgItem(hwnd, IDC_NATIVE_SECONDARY_COMBO);
        HWND bass = GetDlgItem(hwnd, IDC_NATIVE_EQ_BASS);
        HWND mid = GetDlgItem(hwnd, IDC_NATIVE_EQ_MID);
        HWND treble = GetDlgItem(hwnd, IDC_NATIVE_EQ_TREBLE);
        if (!secondary || !bass || !mid || !treble)
            return false;

        SetWindowTextW(GetDlgItem(hwnd, IDC_NATIVE_SECONDARY_LABEL),
                       L"Microfone secund\u00E1rio");
        SetWindowTextW(GetDlgItem(hwnd, IDC_NATIVE_EQ_BASS_LABEL), L"Graves");
        SetWindowTextW(GetDlgItem(hwnd, IDC_NATIVE_EQ_MID_LABEL), L"M\u00E9dios");
        SetWindowTextW(GetDlgItem(hwnd, IDC_NATIVE_EQ_TREBLE_LABEL), L"Agudos");

        SetAccessibleName(secondary, L"Microfone secund\u00E1rio", false);
        SetAccessibleName(bass, L"Graves", true);
        SetAccessibleName(mid, L"M\u00E9dios", true);
        SetAccessibleName(treble, L"Agudos", true);

        LONG_PTR secondaryStyle = GetWindowLongPtrW(secondary, GWL_STYLE);
        SetWindowLongPtrW(secondary, GWL_STYLE, secondaryStyle | WS_TABSTOP);

        HWND output = GetDlgItem(hwnd, IDC_COMBO_OUTPUTDRIVER);
        if (output)
        {
            SetWindowPos(secondary, output, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        return true;
    }

    std::wstring TreeItemText(HWND tree, HTREEITEM item)
    {
        wchar_t text[2048] = {};
        TVITEMW info = {};
        info.mask = TVIF_TEXT;
        info.hItem = item;
        info.pszText = text;
        info.cchTextMax = static_cast<int>(_countof(text));
        if (!TreeView_GetItem(tree, &info))
            return std::wstring();
        return std::wstring(text);
    }

    LPARAM TreeItemData(HWND tree, HTREEITEM item)
    {
        TVITEMW info = {};
        info.mask = TVIF_PARAM;
        info.hItem = item;
        if (!TreeView_GetItem(tree, &info))
            return 0;
        return info.lParam;
    }

    void SetTreeItemText(HWND tree, HTREEITEM item, const std::wstring& text)
    {
        TVITEMW info = {};
        info.mask = TVIF_TEXT;
        info.hItem = item;
        info.pszText = const_cast<wchar_t*>(text.c_str());
        TreeView_SetItem(tree, &info);
    }

    std::wstring DecoratedUserText(const std::wstring& current, const User& user)
    {
        std::wstring text = current;

        const std::wstring femalePrefix = L"\U0001f469 ";
        const std::wstring malePrefix = L"\U0001f468 ";
        const std::wstring femaleSuffix = L" \U0001f469";
        const std::wstring maleSuffix = L" \U0001f468";

        // Classic currently appends the gender emoji. Normalize both the old
        // suffix and this module's prefix so repeated updates stay idempotent.
        StripPrefix(text, femalePrefix);
        StripPrefix(text, malePrefix);
        StripSuffix(text, femaleSuffix);
        StripSuffix(text, maleSuffix);

        CString displayName = GetDisplayName(user);
        std::wstring name(displayName.GetString());
        const std::wstring adminTag = L" (Administrador)";

        if (!name.empty() && StartsWith(text, name + adminTag))
            text.erase(name.size(), adminTag.size());

        if ((user.uUserType & USERTYPE_ADMIN) == USERTYPE_ADMIN &&
            !name.empty() && StartsWith(text, name))
        {
            text.insert(name.size(), adminTag);
        }

        if (user.nStatusMode & STATUSMODE_FEMALE)
            text.insert(0, femalePrefix);
        else if ((user.nStatusMode & STATUSMODE_GENDER_MASK) == STATUSMODE_MALE)
            text.insert(0, malePrefix);

        return text;
    }

    void DecorateTreeBranch(HWND tree, HTREEITEM item)
    {
        while (item)
        {
            LPARAM data = TreeItemData(tree, item);
            if ((data & USER_ITEMDATA) != 0 && ttInst)
            {
                int userId = static_cast<int>(data & ID_ITEMDATA);
                User user = {};
                if (TT_GetUser(ttInst, userId, &user))
                {
                    std::wstring current = TreeItemText(tree, item);
                    std::wstring decorated = DecoratedUserText(current, user);
                    if (!decorated.empty() && decorated != current)
                        SetTreeItemText(tree, item, decorated);
                }
            }

            HTREEITEM child = TreeView_GetChild(tree, item);
            if (child)
                DecorateTreeBranch(tree, child);
            item = TreeView_GetNextSibling(tree, item);
        }
    }

    void DecorateUserTree(HWND tree)
    {
        if (!tree || g_decoratingTree)
            return;

        g_decoratingTree = true;
        HTREEITEM root = TreeView_GetRoot(tree);
        if (root)
            DecorateTreeBranch(tree, root);
        g_decoratingTree = false;
    }

    LRESULT CALLBACK UserTreeWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        TreeState* state = reinterpret_cast<TreeState*>(GetPropW(hwnd, TREE_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                                  reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, TREE_STATE_PROP);
            LRESULT result = previous
                ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        LRESULT result = previous
            ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
            : DefWindowProcW(hwnd, msg, wParam, lParam);

        if (!g_decoratingTree &&
            (msg == TVM_SETITEMW || msg == TVM_INSERTITEMW || msg == TVM_DELETEITEM))
        {
            DecorateUserTree(hwnd);
        }

        return result;
    }

    void SubclassUserTree(HWND tree)
    {
        if (!tree || GetPropW(tree, TREE_STATE_PROP))
            return;

        TreeState* state = new TreeState();
        state->previous = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(tree, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(&UserTreeWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }

        SetPropW(tree, TREE_STATE_PROP, state);
        DecorateUserTree(tree);
    }

    std::wstring Utf8ToWide(const char* text)
    {
        if (!text || !*text)
            return std::wstring();
        CString value = STR_UTF8(text);
        return std::wstring(value.GetString());
    }

    void ParseCountries(const CString& response, CountryState* state)
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

        for (tinyxml2::XMLElement* host = root->FirstChildElement("host");
             host;
             host = host->NextSiblingElement("host"))
        {
            tinyxml2::XMLElement* nameElement = host->FirstChildElement("name");
            tinyxml2::XMLElement* stats = host->FirstChildElement("stats");
            tinyxml2::XMLElement* countryElement =
                stats ? stats->FirstChildElement("country") : nullptr;
            if (!nameElement || !countryElement)
                continue;

            std::wstring name = Utf8ToWide(nameElement->GetText());
            std::wstring country = Utf8ToWide(countryElement->GetText());
            if (name.empty() || country.empty())
                continue;

            tinyxml2::XMLElement* listingElement = host->FirstChildElement("listing");
            const char* listing = listingElement ? listingElement->GetText() : nullptr;
            if (listing && std::string(listing) == "official")
                state->official[name] = country;
            else
                state->unofficial[name] = country;
        }
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

    void DecorateServerList(HWND hwnd, CountryState* state)
    {
        if (!state)
            return;

        HWND list = GetDlgItem(hwnd, IDC_LIST_HOSTS);
        if (!list)
            return;

        int count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
        if (count == LB_ERR || count <= 0)
            return;

        int selected = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        bool changed = false;
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);

        for (int i = 0; i < count; ++i)
        {
            ULONG_PTR itemData = static_cast<ULONG_PTR>(
                SendMessageW(list, LB_GETITEMDATA, static_cast<WPARAM>(i), 0));
            if ((itemData & PUBSERVER_ITEMDATA) == 0)
                continue;

            std::wstring current = ListItemText(list, i);
            if (current.empty())
                continue;

            const std::wstring countryMarker = L", Pa\u00EDs: ";
            size_t marker = current.find(countryMarker);
            std::wstring base = marker == std::wstring::npos
                ? current : current.substr(0, marker);

            const std::wstring officialPrefix = L"Official: ";
            const std::wstring unofficialPrefix = L"Unofficial: ";
            const std::map<std::wstring, std::wstring>* countries = nullptr;
            std::wstring serverName;

            if (StartsWith(base, officialPrefix))
            {
                countries = &state->official;
                serverName = base.substr(officialPrefix.size());
            }
            else if (StartsWith(base, unofficialPrefix))
            {
                countries = &state->unofficial;
                serverName = base.substr(unofficialPrefix.size());
            }
            else
                continue;

            auto country = countries->find(serverName);
            if (country == countries->end())
                continue;

            std::wstring wanted = base + countryMarker + country->second;
            if (wanted == current)
                continue;

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

    void StartCountryLookup(HWND hwnd)
    {
        if (!hwnd || GetPropW(hwnd, COUNTRY_STATE_PROP))
            return;

        CountryState* state = new CountryState();
        CString url = URL_PUBLICSERVER;
        url += _T("&official=1&unofficial=1");
        state->request.reset(new CHttpRequest(url));
        state->started = GetTickCount64();
        SetPropW(hwnd, COUNTRY_STATE_PROP, state);
        SetTimer(hwnd, TIMER_SERVER_COUNTRY, 350, nullptr);
    }

    void PollCountryLookup(HWND hwnd)
    {
        CountryState* state =
            reinterpret_cast<CountryState*>(GetPropW(hwnd, COUNTRY_STATE_PROP));
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
                ParseCountries(response, state);
                state->request.reset();
            }
            else if (GetTickCount64() - state->started > 10000)
            {
                state->request.reset();
            }
        }

        DecorateServerList(hwnd, state);
    }

    bool IsSoundPage(HWND hwnd)
    {
        return GetDlgItem(hwnd, IDC_COMBO_INPUTDRIVER) &&
               GetDlgItem(hwnd, IDC_COMBO_OUTPUTDRIVER) &&
               GetDlgItem(hwnd, IDC_CHECK_ECHOCANCEL);
    }

    bool IsMainWindow(HWND hwnd)
    {
        return GetDlgItem(hwnd, IDC_TREE_SESSION) &&
               GetDlgItem(hwnd, IDC_TAB_CTRL);
    }

    bool IsHostManager(HWND hwnd)
    {
        return GetDlgItem(hwnd, IDC_LIST_HOSTS) &&
               GetDlgItem(hwnd, IDC_CHECK_PUBLICSERVERS) &&
               GetDlgItem(hwnd, IDC_COMBO_HOSTADDRESS);
    }

    LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && lParam)
        {
            const CWPSTRUCT* message = reinterpret_cast<const CWPSTRUCT*>(lParam);
            HWND hwnd = message->hwnd;

            if (message->message == WM_INITDIALOG)
            {
                if (IsSoundPage(hwnd))
                {
                    SetTimer(hwnd, TIMER_SOUND_ACCESSIBILITY, 80, nullptr);
                }
                else if (IsMainWindow(hwnd))
                {
                    SubclassUserTree(GetDlgItem(hwnd, IDC_TREE_SESSION));
                }
                else if (IsHostManager(hwnd))
                {
                    StartCountryLookup(hwnd);
                }
            }

            if (message->message == WM_TIMER)
            {
                if (message->wParam == TIMER_SOUND_ACCESSIBILITY && IsSoundPage(hwnd))
                {
                    if (ApplySoundAccessibility(hwnd))
                        KillTimer(hwnd, TIMER_SOUND_ACCESSIBILITY);
                }
                else if (message->wParam == TIMER_SERVER_COUNTRY &&
                         GetPropW(hwnd, COUNTRY_STATE_PROP))
                {
                    PollCountryLookup(hwnd);
                }
            }

            if (message->message == WM_NCDESTROY)
            {
                if (GetPropW(hwnd, COUNTRY_STATE_PROP))
                {
                    KillTimer(hwnd, TIMER_SERVER_COUNTRY);
                    CountryState* state = reinterpret_cast<CountryState*>(
                        RemovePropW(hwnd, COUNTRY_STATE_PROP));
                    delete state;
                }
                KillTimer(hwnd, TIMER_SOUND_ACCESSIBILITY);
            }
        }

        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
}

namespace NativeAccessibilityFeatures
{
    void Start()
    {
        if (g_hook)
            return;

        g_hook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProcHook,
                                   nullptr, GetCurrentThreadId());
    }

    void Stop()
    {
        if (g_hook)
        {
            UnhookWindowsHookEx(g_hook);
            g_hook = nullptr;
        }
    }
}
