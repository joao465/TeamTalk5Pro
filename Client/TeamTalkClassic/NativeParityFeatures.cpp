#include "stdafx.h"
#include "NativeParityFeatures.h"
#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>

#include <commctrl.h>
#include <ShlObj.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

namespace
{
    constexpr UINT WM_TT_APPLY_DISPLAY_PARITY = WM_APP + 0x351;
    constexpr UINT WM_TT_APPLY_HOST_PARITY = WM_APP + 0x352;
    constexpr UINT WM_TT_APPLY_CHAT_PARITY = WM_APP + 0x353;

    constexpr int IDC_NATIVE_CHAT_LISTVIEW = 60201;
    constexpr int IDC_NATIVE_UNOFFICIAL_SERVERS = 60202;
    constexpr int IDC_NATIVE_HISTORY_LIST = 60203;

    const wchar_t* DISPLAY_STATE_PROP = L"TeamTalkPro.NativeDisplayParity";
    const wchar_t* HOST_STATE_PROP = L"TeamTalkPro.NativeHostParity";
    const wchar_t* CHAT_STATE_PROP = L"TeamTalkPro.NativeChatParity";
    const wchar_t* RICH_STATE_PROP = L"TeamTalkPro.NativeRichParity";

    struct DisplayState
    {
        WNDPROC previous = nullptr;
        HWND checkbox = nullptr;
        bool original = false;
    };

    struct HostState
    {
        WNDPROC previous = nullptr;
        HWND checkbox = nullptr;
    };

    struct ChatState
    {
        WNDPROC previous = nullptr;
        HWND rich = nullptr;
        HWND list = nullptr;
    };

    struct RichState
    {
        WNDPROC previous = nullptr;
        HWND list = nullptr;
    };

    HHOOK g_hook = nullptr;

    std::wstring GetProFolder()
    {
        wchar_t appdata[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr,
                                    SHGFP_TYPE_CURRENT, appdata)))
            return L".";

        std::wstring folder = appdata;
        folder += L"\\TeamTalk 5 Pro";
        CreateDirectoryW(folder.c_str(), nullptr);
        return folder;
    }

    std::wstring NativeProfilePath()
    {
        return GetProFolder() + L"\\TeamTalkProNative.ini";
    }

    int ReadDisplayInt(const wchar_t* key, int fallback)
    {
        return static_cast<int>(GetPrivateProfileIntW(
            L"Display", key, fallback, NativeProfilePath().c_str()));
    }

    void WriteDisplayInt(const wchar_t* key, int value)
    {
        wchar_t text[32] = {};
        _snwprintf_s(text, _countof(text), _TRUNCATE, L"%d", value);
        WritePrivateProfileStringW(L"Display", key, text,
                                   NativeProfilePath().c_str());
    }

    bool ChatHistoryListViewEnabled()
    {
        return ReadDisplayInt(L"ChatHistoryListView", 0) != 0;
    }

    bool UnofficialServersEnabled()
    {
        return ReadDisplayInt(L"ShowUnofficialServers", 0) != 0;
    }

    RECT ChildRect(HWND parent, HWND child)
    {
        RECT rc = {};
        if (!parent || !child)
            return rc;
        GetWindowRect(child, &rc);
        MapWindowPoints(HWND_DESKTOP, parent,
                        reinterpret_cast<LPPOINT>(&rc), 2);
        return rc;
    }

    HFONT DialogFont(HWND hwnd)
    {
        return reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
    }

    void SetControlFont(HWND hwnd, HWND control)
    {
        if (!control)
            return;
        HFONT font = DialogFont(hwnd);
        if (font)
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }

    void ApplyDisplayPage(HWND hwnd)
    {
        DisplayState* state = reinterpret_cast<DisplayState*>(GetPropW(hwnd, DISPLAY_STATE_PROP));
        if (!state || state->checkbox)
            return;

        HWND anchor = GetDlgItem(hwnd, IDC_CHECK_LOGGEDINOUT);
        if (!anchor)
            anchor = GetDlgItem(hwnd, IDC_CHECK_MSGTIMESTAMP);
        if (!anchor)
            return;

        RECT anchorRc = ChildRect(hwnd, anchor);
        RECT sortRc = ChildRect(hwnd, GetDlgItem(hwnd, IDC_COMBO_SORTCHANNELS));
        RECT groupRc = ChildRect(hwnd, GetDlgItem(hwnd, IDC_STATIC_GRPWINDOW));
        RECT client = {};
        GetClientRect(hwnd, &client);

        int x = anchorRc.left;
        int y = std::max(anchorRc.bottom + 6, sortRc.bottom + 4);
        int maxY = groupRc.bottom > 0 ? groupRc.bottom - 28 : client.bottom - 30;
        y = std::min(y, maxY);
        int width = (groupRc.right > x ? groupRc.right : client.right) - x - 8;
        width = std::max(width, 180);

        state->checkbox = CreateWindowExW(
            0, L"BUTTON", L"Show chat history as list view instead of text edit",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE,
            x, y, width, 24, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NATIVE_CHAT_LISTVIEW)),
            AfxGetInstanceHandle(), nullptr);

        if (state->checkbox)
        {
            state->original = ChatHistoryListViewEnabled();
            SendMessageW(state->checkbox, BM_SETCHECK,
                         state->original ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlFont(hwnd, state->checkbox);
            SetWindowPos(state->checkbox, anchor, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    LRESULT CALLBACK DisplayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        DisplayState* state = reinterpret_cast<DisplayState*>(GetPropW(hwnd, DISPLAY_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_TT_APPLY_DISPLAY_PARITY)
        {
            ApplyDisplayPage(hwnd);
            return 0;
        }

        if (msg == WM_NOTIFY && lParam)
        {
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr->code == PSN_APPLY)
            {
                LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                          : DefWindowProcW(hwnd, msg, wParam, lParam);
                if (state && state->checkbox &&
                    GetWindowLongPtrW(hwnd, DWLP_MSGRESULT) == PSNRET_NOERROR)
                {
                    bool enabled = SendMessageW(state->checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
                    if (enabled != state->original)
                    {
                        WriteDisplayInt(L"ChatHistoryListView", enabled ? 1 : 0);
                        state->original = enabled;
                        MessageBoxW(hwnd,
                            L"Restart TeamTalk Pro to change the chat history control.",
                            L"Chat History", MB_OK | MB_ICONINFORMATION);
                    }
                }
                return result;
            }
        }

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, DISPLAY_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void SubclassDisplayPage(HWND hwnd)
    {
        if (GetPropW(hwnd, DISPLAY_STATE_PROP))
            return;

        DisplayState* state = new DisplayState();
        state->previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&DisplayWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }
        SetPropW(hwnd, DISPLAY_STATE_PROP, state);
        PostMessageW(hwnd, WM_TT_APPLY_DISPLAY_PARITY, 0, 0);
    }

    void ApplyHostManager(HWND hwnd)
    {
        HostState* state = reinterpret_cast<HostState*>(GetPropW(hwnd, HOST_STATE_PROP));
        if (!state || state->checkbox)
            return;

        HWND official = GetDlgItem(hwnd, IDC_CHECK_PUBLICSERVERS);
        HWND group = GetDlgItem(hwnd, IDC_STATIC_GRPHOSTS);
        if (!official || !group)
            return;

        SetWindowTextW(official, L"Official servers");
        RECT row = ChildRect(hwnd, official);
        RECT groupRc = ChildRect(hwnd, group);
        const int gap = 8;
        int left = row.left;
        int right = groupRc.right - 8;
        int width = std::max(70, (right - left - gap) / 2);
        int height = std::max(20, row.bottom - row.top + 6);

        MoveWindow(official, left, row.top, width, height, TRUE);
        state->checkbox = CreateWindowExW(
            0, L"BUTTON", L"Unofficial servers",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            left + width + gap, row.top, width, height, hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NATIVE_UNOFFICIAL_SERVERS)),
            AfxGetInstanceHandle(), nullptr);

        if (state->checkbox)
        {
            SendMessageW(state->checkbox, BM_SETCHECK,
                         UnofficialServersEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlFont(hwnd, state->checkbox);
            SetWindowPos(state->checkbox, official, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }

    LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        HostState* state = reinterpret_cast<HostState*>(GetPropW(hwnd, HOST_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_TT_APPLY_HOST_PARITY)
        {
            ApplyHostManager(hwnd);
            return 0;
        }

        if (msg == WM_COMMAND && LOWORD(wParam) == IDC_NATIVE_UNOFFICIAL_SERVERS &&
            HIWORD(wParam) == BN_CLICKED)
        {
            bool enabled = state && state->checkbox &&
                           SendMessageW(state->checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
            WriteDisplayInt(L"ShowUnofficialServers", enabled ? 1 : 0);

            HWND official = GetDlgItem(hwnd, IDC_CHECK_PUBLICSERVERS);
            if (previous && official)
                return CallWindowProcW(previous, hwnd, WM_COMMAND,
                                       MAKEWPARAM(IDC_CHECK_PUBLICSERVERS, BN_CLICKED),
                                       reinterpret_cast<LPARAM>(official));
            return 0;
        }

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, HOST_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void SubclassHostManager(HWND hwnd)
    {
        if (GetPropW(hwnd, HOST_STATE_PROP))
            return;

        HostState* state = new HostState();
        state->previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&HostWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }
        SetPropW(hwnd, HOST_STATE_PROP, state);
        PostMessageW(hwnd, WM_TT_APPLY_HOST_PARITY, 0, 0);
    }

    void CopySelectedListItem(HWND list)
    {
        int selection = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        if (selection == LB_ERR)
            return;
        int length = static_cast<int>(SendMessageW(list, LB_GETTEXTLEN, selection, 0));
        if (length < 0)
            return;

        std::vector<wchar_t> text(static_cast<size_t>(length) + 1, L'\0');
        SendMessageW(list, LB_GETTEXT, selection, reinterpret_cast<LPARAM>(text.data()));

        if (!OpenClipboard(list))
            return;
        EmptyClipboard();
        SIZE_T bytes = (static_cast<SIZE_T>(length) + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (memory)
        {
            void* target = GlobalLock(memory);
            if (target)
            {
                memcpy(target, text.data(), bytes);
                GlobalUnlock(memory);
                if (!SetClipboardData(CF_UNICODETEXT, memory))
                    GlobalFree(memory);
            }
            else
                GlobalFree(memory);
        }
        CloseClipboard();
    }

    LRESULT CALLBACK HistoryListSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                         UINT_PTR subclassId, DWORD_PTR)
    {
        if (msg == WM_KEYDOWN && (wParam == L'C' || wParam == L'c') &&
            (GetKeyState(VK_CONTROL) & 0x8000))
        {
            CopySelectedListItem(hwnd);
            return 0;
        }
        if (msg == WM_NCDESTROY)
            RemoveWindowSubclass(hwnd, HistoryListSubclass, subclassId);
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }

    void SyncHistoryList(HWND rich, HWND list)
    {
        if (!rich || !list)
            return;

        int length = GetWindowTextLengthW(rich);
        std::vector<wchar_t> buffer(static_cast<size_t>(std::max(0, length)) + 1, L'\0');
        if (length > 0)
            GetWindowTextW(rich, buffer.data(), length + 1);

        int oldSelection = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        bool listFocused = GetFocus() == list;

        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        SendMessageW(list, LB_RESETCONTENT, 0, 0);

        std::wstring line;
        for (int i = 0; i <= length; ++i)
        {
            wchar_t ch = (i < length) ? buffer[static_cast<size_t>(i)] : L'\n';
            if (ch == L'\r' || ch == L'\n')
            {
                if (!line.empty())
                {
                    SendMessageW(list, LB_ADDSTRING, 0,
                                 reinterpret_cast<LPARAM>(line.c_str()));
                    line.clear();
                }
                if (ch == L'\r' && i + 1 < length &&
                    buffer[static_cast<size_t>(i + 1)] == L'\n')
                    ++i;
            }
            else
                line.push_back(ch);
        }

        int count = static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0));
        if (count > 0)
        {
            int selection = count - 1;
            if (listFocused && oldSelection >= 0)
                selection = std::min(oldSelection, count - 1);
            SendMessageW(list, LB_SETCURSEL, selection, 0);

            RECT rc = {};
            GetClientRect(list, &rc);
            int itemHeight = static_cast<int>(SendMessageW(list, LB_GETITEMHEIGHT, 0, 0));
            int visible = itemHeight > 0 ? std::max(1, (rc.bottom - rc.top) / itemHeight) : 1;
            SendMessageW(list, LB_SETTOPINDEX, std::max(0, count - visible), 0);
        }

        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(list, nullptr, TRUE);
    }

    LRESULT CALLBACK RichHistoryWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        RichState* state = reinterpret_cast<RichState*>(GetPropW(hwnd, RICH_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, RICH_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        bool changesText = msg == WM_SETTEXT || msg == EM_REPLACESEL ||
                           msg == EM_STREAMIN || msg == WM_CLEAR;
        LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                  : DefWindowProcW(hwnd, msg, wParam, lParam);
        if (changesText && state)
            SyncHistoryList(hwnd, state->list);
        return result;
    }

    void LayoutChatHistory(HWND hwnd, ChatState* state)
    {
        if (!state || !state->rich || !state->list)
            return;
        RECT rc = ChildRect(hwnd, state->rich);
        MoveWindow(state->list, rc.left, rc.top,
                   std::max(1, rc.right - rc.left),
                   std::max(1, rc.bottom - rc.top), TRUE);
    }

    void ApplyChatHistory(HWND hwnd)
    {
        ChatState* state = reinterpret_cast<ChatState*>(GetPropW(hwnd, CHAT_STATE_PROP));
        if (!state || state->list || !state->rich)
            return;

        RECT rc = ChildRect(hwnd, state->rich);
        state->list = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"LISTBOX", L"Chat history",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
            LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
            rc.left, rc.top, std::max(1, rc.right - rc.left),
            std::max(1, rc.bottom - rc.top), hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NATIVE_HISTORY_LIST)),
            AfxGetInstanceHandle(), nullptr);
        if (!state->list)
            return;

        SetControlFont(hwnd, state->list);
        SetWindowTheme(state->list, L"Explorer", nullptr);
        SetWindowSubclass(state->list, HistoryListSubclass, 1, 0);
        SetWindowPos(state->list, state->rich, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

        RichState* richState = new RichState();
        richState->list = state->list;
        richState->previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            state->rich, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(&RichHistoryWndProc)));
        if (!richState->previous)
        {
            delete richState;
            DestroyWindow(state->list);
            state->list = nullptr;
            return;
        }
        SetPropW(state->rich, RICH_STATE_PROP, richState);

        SyncHistoryList(state->rich, state->list);
        ShowWindow(state->rich, SW_HIDE);
    }

    LRESULT CALLBACK ChatWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        ChatState* state = reinterpret_cast<ChatState*>(GetPropW(hwnd, CHAT_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_TT_APPLY_CHAT_PARITY)
        {
            ApplyChatHistory(hwnd);
            return 0;
        }

        if (msg == WM_SIZE)
        {
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            LayoutChatHistory(hwnd, state);
            return result;
        }

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, CHAT_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void SubclassChatHistory(HWND hwnd, int richId)
    {
        if (!ChatHistoryListViewEnabled() || GetPropW(hwnd, CHAT_STATE_PROP))
            return;

        HWND rich = GetDlgItem(hwnd, richId);
        if (!rich)
            return;

        ChatState* state = new ChatState();
        state->rich = rich;
        state->previous = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
            hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&ChatWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }
        SetPropW(hwnd, CHAT_STATE_PROP, state);
        PostMessageW(hwnd, WM_TT_APPLY_CHAT_PARITY, 0, 0);
    }

    LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && lParam)
        {
            const CWPSTRUCT* message = reinterpret_cast<const CWPSTRUCT*>(lParam);
            if (message->message == WM_INITDIALOG)
            {
                HWND hwnd = message->hwnd;
                if (GetDlgItem(hwnd, IDC_CHECK_MSGTIMESTAMP) &&
                    GetDlgItem(hwnd, IDC_CHECK_LOGGEDINOUT) &&
                    GetDlgItem(hwnd, IDC_COMBO_SORTCHANNELS))
                {
                    SubclassDisplayPage(hwnd);
                }
                else if (GetDlgItem(hwnd, IDC_CHECK_PUBLICSERVERS) &&
                         GetDlgItem(hwnd, IDC_LIST_HOSTS) &&
                         GetDlgItem(hwnd, IDC_COMBO_HOSTADDRESS))
                {
                    SubclassHostManager(hwnd);
                }
                else if (GetDlgItem(hwnd, IDC_RICHEDIT_CHANMESSAGES) &&
                         GetDlgItem(hwnd, IDC_EDIT_CHANMESSAGE))
                {
                    SubclassChatHistory(hwnd, IDC_RICHEDIT_CHANMESSAGES);
                }
                else if (GetDlgItem(hwnd, IDC_RICHEDIT_HISTORY))
                {
                    SubclassChatHistory(hwnd, IDC_RICHEDIT_HISTORY);
                }
            }
        }
        return CallNextHookEx(g_hook, code, wParam, lParam);
    }
}

namespace NativeParityFeatures
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

    bool ShowUnofficialServers()
    {
        return UnofficialServersEnabled();
    }
}
