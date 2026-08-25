#include "stdafx.h"
#include "ProNativeModernUI.h"
#include "ProNativeRuntime.h"
#include "TeamTalkDlg.h"
#include "resource.h"

#include <atomic>
#include <thread>

#include <uxtheme.h>
#pragma comment(lib, "uxtheme.lib")

namespace
{
    constexpr UINT ID_PRO_AUDIO_SETTINGS = 50001;
    constexpr UINT ID_PRO_PREFERENCES = 50002;
    constexpr UINT ID_PRO_ABOUT_NATIVE = 50003;

    std::atomic<bool> g_running{ false };
    std::thread g_worker;
    CTeamTalkDlg* g_dialog = nullptr;
    HWND g_owner = nullptr;
    WNDPROC g_previousWndProc = nullptr;
    HMENU g_proMenu = nullptr;

    void ApplyNativeThemeToChildren(HWND owner)
    {
        EnumChildWindows(owner, [](HWND child, LPARAM) -> BOOL
        {
            wchar_t cls[64] = {};
            GetClassNameW(child, cls, static_cast<int>(std::size(cls)));

            if (_wcsicmp(cls, WC_TREEVIEWW) == 0 || _wcsicmp(cls, WC_LISTVIEWW) == 0)
                SetWindowTheme(child, L"Explorer", nullptr);
            else if (_wcsicmp(cls, WC_TABCONTROLW) == 0)
                SetWindowTheme(child, L"Tab", nullptr);

            return TRUE;
        }, 0);
    }

    void AddProMenu(HWND owner)
    {
        HMENU mainMenu = GetMenu(owner);
        if (!mainMenu || g_proMenu)
            return;

        g_proMenu = CreatePopupMenu();
        if (!g_proMenu)
            return;

        AppendMenuW(g_proMenu, MF_STRING, ID_PRO_AUDIO_SETTINGS,
                    L"&Áudio Pro...\tCtrl+Alt+A");
        AppendMenuW(g_proMenu, MF_STRING, ID_PRO_PREFERENCES,
                    L"&Preferências do TeamTalk...");
        AppendMenuW(g_proMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(g_proMenu, MF_STRING, ID_PRO_ABOUT_NATIVE,
                    L"&Sobre o cliente nativo...");

        AppendMenuW(mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(g_proMenu),
                    L"TeamTalk &Pro");
        DrawMenuBar(owner);
    }

    LRESULT CALLBACK ModernWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_COMMAND)
        {
            switch (LOWORD(wParam))
            {
            case ID_PRO_AUDIO_SETTINGS:
                ProNativeRuntime::ShowAudioSettings(hwnd);
                return 0;
            case ID_PRO_PREFERENCES:
                SendMessageW(hwnd, WM_COMMAND, ID_FILE_PREFERENCES, 0);
                return 0;
            case ID_PRO_ABOUT_NATIVE:
                MessageBoxW(hwnd,
                    L"TeamTalk 5 Pro - Cliente Nativo\n\n"
                    L"Interface nativa do Windows baseada no TeamTalk Classic, "
                    L"mantida pelo TeamTalk Pro e atualizada para usar o SDK atual.\n\n"
                    L"Esta edição não utiliza o runtime do Qt. As funções do cliente Qt "
                    L"estão sendo migradas progressivamente para esta interface.",
                    L"TeamTalk 5 Pro", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
        }
        else if (msg == WM_KEYDOWN && wParam == 'A')
        {
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            if (ctrl && alt)
            {
                ProNativeRuntime::ShowAudioSettings(hwnd);
                return 0;
            }
        }
        else if (msg == WM_NCDESTROY)
        {
            WNDPROC previous = g_previousWndProc;
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            g_previousWndProc = nullptr;
            g_owner = nullptr;
            return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                            : DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        return g_previousWndProc
            ? CallWindowProcW(g_previousWndProc, hwnd, msg, wParam, lParam)
            : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void WorkerMain()
    {
        HWND owner = nullptr;
        for (int i = 0; g_running && i < 200; ++i)
        {
            if (g_dialog && IsWindow(g_dialog->GetSafeHwnd()))
            {
                owner = g_dialog->GetSafeHwnd();
                break;
            }
            Sleep(50);
        }

        if (!g_running || !owner)
            return;

        g_owner = owner;
        ApplyNativeThemeToChildren(owner);
        AddProMenu(owner);

        // Keep the existing MFC routing intact. We only intercept TeamTalk Pro
        // commands and then forward every other message to the original proc.
        g_previousWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(owner, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(&ModernWndProc)));

        while (g_running && IsWindow(owner))
        {
            // New dialogs/controls can appear after connection. Reapply the
            // native Windows theme occasionally without changing layout.
            ApplyNativeThemeToChildren(owner);
            for (int i = 0; g_running && i < 20; ++i)
                Sleep(100);
        }
    }
}

namespace ProNativeModernUI
{
    void Start(CTeamTalkDlg* dialog)
    {
        if (g_running.exchange(true))
            return;
        g_dialog = dialog;
        g_worker = std::thread(WorkerMain);
    }

    void Stop()
    {
        if (!g_running.exchange(false))
            return;

        if (g_worker.joinable())
            g_worker.join();

        if (g_owner && IsWindow(g_owner) && g_previousWndProc)
        {
            SetWindowLongPtrW(g_owner, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_previousWndProc));
            g_previousWndProc = nullptr;
        }

        g_owner = nullptr;
        g_dialog = nullptr;
        g_proMenu = nullptr;
    }
}
