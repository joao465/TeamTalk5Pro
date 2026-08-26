/*
 * Copyright (c) 2005-2018, BearWare.dk
 * 
 * Contact Information:
 *
 * Bjoern D. Rasmussen
 * Kirketoften 5
 * DK-8260 Viby J
 * Denmark
 * Email: contact@bearware.dk
 * Phone: +45 20 20 54 59
 * Web: http://www.bearware.dk
 *
 * This source code is part of the TeamTalk SDK owned by
 * BearWare.dk. Use of this file, or its compiled unit, requires a
 * TeamTalk SDK License Key issued by BearWare.dk.
 *
 * The TeamTalk SDK License Agreement along with its Terms and
 * Conditions are outlined in the file License.txt included with the
 * TeamTalk SDK distribution.
 *
 */

#include "stdafx.h"
#include "TeamTalkApp.h"
#include "TeamTalkDlg.h"
#include "AppInfo.h"
#include "License.h"
#include "ProNativeRuntime.h"
#include "NativeWindowsUI.h"
#include "NativeParityFeatures.h"
#include <VersionHelpers.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

extern "C"
{
    LASTINPUT GetLastInputInfo_ = NULL;

    DWORD GetLastInput()
    {
        DWORD dwResult = 0;
        if(GetLastInputInfo_)
        {
            LASTINPUTINFO info;
            memset(&info, 0, sizeof(info));
            info.cbSize = sizeof(LASTINPUTINFO);
            if(GetLastInputInfo_(&info))
                dwResult = info.dwTime;
            else
            {
                ASSERT(FALSE);
                dwResult = ::GetTickCount();
            }
        }
        return dwResult;
    }
}

typedef struct
{
    HWND hwnd;
    LPCTSTR title;
} FindWnd;

BOOL CALLBACK CheckWindowTitle(HWND hwnd, LPARAM lParam)
{
    TCHAR buffer[MAX_PATH];
    GetWindowText(hwnd, buffer, MAX_PATH);
    FindWnd* fw = (FindWnd*)lParam;
    if(_tcsncmp(buffer, fw->title, MAX_PATH) == 0)
    {
        fw->hwnd = hwnd;
        return FALSE;
    }
    return TRUE;
}

HWND FindWinTitle(LPCTSTR title)
{
    FindWnd fw;
    fw.hwnd = 0;
    fw.title = title;
    EnumWindows((WNDENUMPROC)CheckWindowTitle, (LPARAM)&fw);
    return fw.hwnd;
}

BOOL IsWin2kPlus()
{
    return IsWindowsXPOrGreater();
}

void MyCommandLineInfo::ParseParam(LPCTSTR lpszParam, BOOL bFlag, BOOL bLast)
{
    bFlag = bFlag;
    bLast = bLast;
    m_args.AddTail(lpszParam);
}

BEGIN_MESSAGE_MAP(CTeamTalkApp, CWinApp)
END_MESSAGE_MAP()

CTeamTalkApp::CTeamTalkApp()
{
}

CTeamTalkApp theApp;

const GUID CDECL BASED_CODE _tlid =
{ 0xBC4E1CC0, 0x8B80, 0x4BD4, { 0x85, 0xC3, 0xD1, 0xAC, 0x38, 0x6A, 0x29, 0xB8 } };
const WORD _wVerMajor = 1;
const WORD _wVerMinor = 0;

BOOL CTeamTalkApp::InitInstance()
{
    AfxOleInit();
    InitCommonControls();
    CWinApp::InitInstance();
    AfxEnableControlContainer();

    HMODULE hCoreMod = 0;
    if(IsWin2kPlus())
    {
        hCoreMod = LoadLibrary(_T("USER32.dll"));
        GetLastInputInfo_ = (LASTINPUT)GetProcAddress(hCoreMod, "GetLastInputInfo");
        ASSERT(GetLastInputInfo_);
    }

    if(!AfxInitRichEdit2())
        AfxMessageBox(_T("Failed to initialize RichEdit component"));

    SetCurrentDirectory(GetExecutableFolder());
    TT_SetLicenseInformation(REGISTRATION_NAME, REGISTRATION_KEY);

    HWND hRunningTT = FindWinTitle(APPTITLE);

    MyCommandLineInfo info;
    ParseCommandLine(info);

    // Keep the diagnostic command-line entry point. Normal users configure
    // these settings inline on the Sound System preferences page.
    for(POSITION p = info.m_args.GetHeadPosition(); p != NULL;)
    {
        CString arg = info.m_args.GetNext(p);
        if(arg.CompareNoCase(_T("--pro-audio-settings")) == 0)
        {
            ProNativeRuntime::MigrateQtSettings();
            ProNativeRuntime::ShowAudioSettings(NULL);
            if(hCoreMod) FreeLibrary(hCoreMod);
            return FALSE;
        }
    }

    // One-time Qt -> native migration. The old INI is kept as a safety backup.
    ProNativeRuntime::MigrateQtSettings();

    CString szArg;
    if(hRunningTT && info.m_args.GetCount() == 1)
    {
        BOOL bTTUrl = FALSE;
        MsgCmdLine msg = {};
        for(POSITION pos = info.m_args.GetHeadPosition(); pos != NULL;)
        {
            szArg = info.m_args.GetNext(pos);
            _tcsncat(msg.szPath, szArg.GetBuffer(), MAX_PATH);
            _tcsncat(msg.szPath, _T("¤"), MAX_PATH);
            bTTUrl |= StartsWith(szArg, TTURL, FALSE);
            bTTUrl |= EndsWith(szArg, _T(TTFILE_EXT), FALSE);
        }

        if(bTTUrl)
        {
            COPYDATASTRUCT cds;
            cds.dwData = 0;
            cds.cbData = sizeof(msg);
            cds.lpData = &msg;
            ::SendMessage(hRunningTT, WM_COPYDATA, 0, (LPARAM)&cds);
            if(hCoreMod) FreeLibrary(hCoreMod);
            return FALSE;
        }
    }

    szArg.Empty();
    CTeamTalkDlg dlg;
    dlg.m_cmdArgs.AddHead(&info.m_args);
    m_pMainWnd = &dlg;

    ProNativeRuntime::Start(&dlg);
    // Install the native UI hook on this same MFC UI thread. Unlike the old
    // experimental implementation, no worker thread mutates HWNDs and no
    // separate "TeamTalk Pro" menu is injected.
    NativeWindowsUI::Start();
    NativeParityFeatures::Start();
    INT_PTR nResponse = dlg.DoModal();
    NativeParityFeatures::Stop();
    NativeWindowsUI::Stop();
    ProNativeRuntime::Stop();

    if(nResponse == IDOK)
    {
    }
    else if(nResponse == IDCANCEL)
    {
    }

    if(hCoreMod)
        FreeLibrary(hCoreMod);
    hCoreMod = 0;
    return FALSE;
}

int CTeamTalkApp::ExitInstance()
{
    NativeParityFeatures::Stop();
    NativeWindowsUI::Stop();
    ProNativeRuntime::Stop();
    return CWinApp::ExitInstance();
}
