#include "stdafx.h"
#include "NativeWindowsUI.h"
#include "Helper.h"
#include "resource.h"

#include <algorithm>
#include <string>
#include <vector>

#include <commctrl.h>
#include <ShlObj.h>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")

extern TTInstance* ttInst;

namespace
{
    constexpr UINT WM_TT_APPLY_MODERN_UI = WM_APP + 0x341;
    constexpr int PRO_SECONDARY_DISABLED = -32768;

    constexpr int IDC_NATIVE_CHANNELS_HEADING = 60001;
    constexpr int IDC_NATIVE_CONTENT_HEADING = 60002;
    constexpr int IDC_NATIVE_VOLUME_LABEL = 60003;
    constexpr int IDC_NATIVE_MIC_LABEL = 60004;
    constexpr int IDC_NATIVE_VOICEACT_LABEL = 60005;
    constexpr int IDC_NATIVE_LEVEL_LABEL = 60006;

    constexpr int IDC_NATIVE_SECONDARY_LABEL = 60101;
    constexpr int IDC_NATIVE_SECONDARY_COMBO = 60102;
    constexpr int IDC_NATIVE_EQ_HEADING = 60103;
    constexpr int IDC_NATIVE_EQ_BASS_LABEL = 60104;
    constexpr int IDC_NATIVE_EQ_BASS = 60105;
    constexpr int IDC_NATIVE_EQ_MID_LABEL = 60106;
    constexpr int IDC_NATIVE_EQ_MID = 60107;
    constexpr int IDC_NATIVE_EQ_TREBLE_LABEL = 60108;
    constexpr int IDC_NATIVE_EQ_TREBLE = 60109;

    const wchar_t* MAIN_STATE_PROP = L"TeamTalkPro.NativeMainState";
    const wchar_t* SOUND_STATE_PROP = L"TeamTalkPro.NativeSoundState";

    struct MainState
    {
        WNDPROC previous = nullptr;
        bool applied = false;
    };

    struct SoundState
    {
        WNDPROC previous = nullptr;
        HWND secondary = nullptr;
        HWND bass = nullptr;
        HWND mid = nullptr;
        HWND treble = nullptr;
    };

    HHOOK g_callWndHook = nullptr;
    HFONT g_uiFont = nullptr;
    HFONT g_headingFont = nullptr;

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

    int ReadProfileInt(const wchar_t* key, int fallback)
    {
        return static_cast<int>(GetPrivateProfileIntW(
            L"Audio", key, fallback, NativeProfilePath().c_str()));
    }

    void WriteProfileInt(const wchar_t* key, int value)
    {
        wchar_t text[32] = {};
        _snwprintf_s(text, _countof(text), _TRUNCATE, L"%d", value);
        WritePrivateProfileStringW(L"Audio", key, text,
                                   NativeProfilePath().c_str());
    }

    void EnsureFonts(HWND hwnd)
    {
        if (g_uiFont && g_headingFont)
            return;

        HDC dc = GetDC(hwnd);
        int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc)
            ReleaseDC(hwnd, dc);

        g_uiFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL,
                               FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                               L"Segoe UI");
        g_headingFont = CreateFontW(-MulDiv(10, dpi, 72), 0, 0, 0, FW_SEMIBOLD,
                                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                    L"Segoe UI");
    }

    BOOL CALLBACK ApplyFontProc(HWND child, LPARAM font)
    {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
        return TRUE;
    }

    void ApplyFontToTree(HWND hwnd)
    {
        EnsureFonts(hwnd);
        SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
        EnumChildWindows(hwnd, ApplyFontProc, reinterpret_cast<LPARAM>(g_uiFont));
    }

    HWND EnsureStatic(HWND parent, int id, const CString& text, bool heading)
    {
        HWND ctrl = GetDlgItem(parent, id);
        if (!ctrl)
        {
            ctrl = CreateWindowExW(0, L"STATIC", text,
                                   WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                   0, 0, 10, 10, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   AfxGetInstanceHandle(), nullptr);
        }
        else
            SetWindowText(ctrl, text);

        if (ctrl)
            SendMessageW(ctrl, WM_SETFONT,
                         reinterpret_cast<WPARAM>(heading ? g_headingFont : g_uiFont), TRUE);
        return ctrl;
    }

    void MoveControl(HWND parent, int id, int x, int y, int width, int height,
                     bool show = true)
    {
        HWND ctrl = GetDlgItem(parent, id);
        if (!ctrl)
            return;
        ShowWindow(ctrl, show ? SW_SHOW : SW_HIDE);
        if (show)
            MoveWindow(ctrl, x, y, std::max(1, width), std::max(1, height), TRUE);
    }

    void PlaceAfter(HWND control, HWND& anchor)
    {
        if (!control)
            return;

        if (anchor)
        {
            SetWindowPos(control, anchor, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        anchor = control;
    }

    CString JoinLabel(UINT firstId, LPCTSTR firstFallback,
                      UINT secondId, LPCTSTR secondFallback)
    {
        CString result = LoadText(firstId, firstFallback);
        result += _T(" / ");
        result += LoadText(secondId, secondFallback);
        return result;
    }

    void LayoutMainWindow(HWND hwnd)
    {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (width < 400 || height < 280)
            return;

        const int margin = 14;
        const int gap = 14;
        const int headingHeight = 24;
        const int bottomHeight = 86;
        const int contentTop = margin + headingHeight;
        const int contentBottom = height - bottomHeight - margin;
        const int contentHeight = std::max(80, contentBottom - contentTop);
        const int usableWidth = width - margin * 2 - gap;
        int leftWidth = static_cast<int>(usableWidth * 0.36);
        leftWidth = std::max(230, std::min(leftWidth, usableWidth - 300));
        const int rightX = margin + leftWidth + gap;
        const int rightWidth = width - rightX - margin;

        MoveControl(hwnd, IDC_NATIVE_CHANNELS_HEADING,
                    margin, margin, leftWidth, headingHeight);
        MoveControl(hwnd, IDC_NATIVE_CONTENT_HEADING,
                    rightX, margin, rightWidth, headingHeight);
        MoveControl(hwnd, IDC_TREE_SESSION,
                    margin, contentTop, leftWidth, contentHeight);
        MoveControl(hwnd, IDC_TAB_CTRL,
                    rightX, contentTop, rightWidth, contentHeight);

        // The old Classic bitmap strip is replaced by a labelled, keyboard-friendly
        // Windows control strip across the bottom of the window.
        MoveControl(hwnd, IDC_STATIC_VOLUME, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_MIKE, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_VOICEACT, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_VU, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_SPLITTER, 0, 0, 0, 0, false);

        const int stripY = height - bottomHeight + 8;
        const int stripWidth = width - margin * 2;
        const int colGap = 12;
        const int colWidth = (stripWidth - colGap * 3) / 4;
        const int labelHeight = 20;
        const int controlHeight = 28;

        const int x1 = margin;
        const int x2 = x1 + colWidth + colGap;
        const int x3 = x2 + colWidth + colGap;
        const int x4 = x3 + colWidth + colGap;

        MoveControl(hwnd, IDC_NATIVE_VOLUME_LABEL, x1, stripY, colWidth, labelHeight);
        MoveControl(hwnd, IDC_SLIDER_VOLUME, x1, stripY + labelHeight, colWidth, controlHeight);

        MoveControl(hwnd, IDC_NATIVE_MIC_LABEL, x2, stripY, colWidth, labelHeight);
        MoveControl(hwnd, IDC_SLIDER_GAINLEVEL, x2, stripY + labelHeight, colWidth, controlHeight);

        MoveControl(hwnd, IDC_NATIVE_VOICEACT_LABEL, x3, stripY, colWidth, labelHeight);
        MoveControl(hwnd, IDC_SLIDER_VOICEACT, x3, stripY + labelHeight, colWidth, controlHeight);

        MoveControl(hwnd, IDC_NATIVE_LEVEL_LABEL, x4, stripY, colWidth, labelHeight);
        MoveControl(hwnd, IDC_PROGRESS_VOICEACT, x4, stripY + labelHeight + 6,
                    colWidth, 16);
    }

    void ApplyMainWindow(HWND hwnd)
    {
        MainState* state = reinterpret_cast<MainState*>(GetPropW(hwnd, MAIN_STATE_PROP));
        if (!state || state->applied)
            return;
        state->applied = true;

        ApplyFontToTree(hwnd);

        CString channelsUsers = JoinLabel(IDS_CHANNEL, _T("Channels"),
                                          IDS_USER, _T("Users"));
        CString content = JoinLabel(IDS_CHAT, _T("Chat"),
                                    IDS_FILES, _T("Files"));
        EnsureStatic(hwnd, IDC_NATIVE_CHANNELS_HEADING, channelsUsers, true);
        EnsureStatic(hwnd, IDC_NATIVE_CONTENT_HEADING, content, true);
        EnsureStatic(hwnd, IDC_NATIVE_VOLUME_LABEL,
                     LoadText(IDS_MASTERVOL, _T("Master volume")), false);
        EnsureStatic(hwnd, IDC_NATIVE_MIC_LABEL,
                     LoadText(IDS_MICGAIN, _T("Microphone gain")), false);
        EnsureStatic(hwnd, IDC_NATIVE_VOICEACT_LABEL,
                     LoadText(IDS_VOICEACTLEVEL, _T("Voice activation")), false);
        EnsureStatic(hwnd, IDC_NATIVE_LEVEL_LABEL,
                     LoadText(IDS_VOICELEVEL, _T("Voice level")), false);

        HWND tree = GetDlgItem(hwnd, IDC_TREE_SESSION);
        HWND tab = GetDlgItem(hwnd, IDC_TAB_CTRL);
        if (tree)
        {
            SetWindowTheme(tree, L"Explorer", nullptr);
#ifdef TVS_EX_DOUBLEBUFFER
            TreeView_SetExtendedStyle(tree, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);
#endif
        }
        if (tab)
            SetWindowTheme(tab, L"Explorer", nullptr);

        // Classic used a very small default dialog. Give the rebuilt workspace
        // enough room to behave like the current TeamTalk client.
        RECT wr = {};
        GetWindowRect(hwnd, &wr);
        const int currentWidth = wr.right - wr.left;
        const int currentHeight = wr.bottom - wr.top;
        const int targetWidth = std::max(currentWidth, 900);
        const int targetHeight = std::max(currentHeight, 600);
        if (targetWidth != currentWidth || targetHeight != currentHeight)
        {
            SetWindowPos(hwnd, nullptr, 0, 0, targetWidth, targetHeight,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        LayoutMainWindow(hwnd);
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    std::vector<SoundDevice> GetInputDevices()
    {
        int count = 0;
        std::vector<SoundDevice> devices;
        TT_GetSoundDevices(nullptr, &count);
        if (count > 0)
        {
            devices.resize(static_cast<size_t>(count));
            TT_GetSoundDevices(&devices[0], &count);
            devices.resize(static_cast<size_t>(std::max(0, count)));
        }

        devices.erase(std::remove_if(devices.begin(), devices.end(),
            [](const SoundDevice& device)
            {
                return device.nMaxInputChannels <= 0 ||
                       device.nDeviceID == TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL;
            }), devices.end());
        return devices;
    }

    void FillSecondaryInput(HWND combo)
    {
        if (!combo)
            return;

        SendMessageW(combo, CB_RESETCONTENT, 0, 0);
        CString disabled = LoadText(IDS_DISABLED, _T("Disabled"));
        int index = static_cast<int>(SendMessage(combo, CB_ADDSTRING, 0,
                                                 reinterpret_cast<LPARAM>(disabled.GetString())));
        SendMessage(combo, CB_SETITEMDATA, index,
                    static_cast<LPARAM>(PRO_SECONDARY_DISABLED));

        const int wanted = ReadProfileInt(L"SecondaryInputDevice",
                                          PRO_SECONDARY_DISABLED);
        int selected = wanted == PRO_SECONDARY_DISABLED ? index : -1;

        std::vector<SoundDevice> devices = GetInputDevices();
        for (size_t i = 0; i < devices.size(); ++i)
        {
            const SoundDevice& device = devices[i];
            int item = static_cast<int>(SendMessage(combo, CB_ADDSTRING, 0,
                                                    reinterpret_cast<LPARAM>(device.szDeviceName)));
            SendMessage(combo, CB_SETITEMDATA, item,
                        static_cast<LPARAM>(device.nDeviceID));
            if (device.nDeviceID == wanted)
                selected = item;
        }

        if (selected < 0)
            selected = 0;
        SendMessage(combo, CB_SETCURSEL, selected, 0);
    }

    HWND EnsureTrackbar(HWND parent, int id, int value)
    {
        HWND ctrl = GetDlgItem(parent, id);
        if (!ctrl)
        {
            ctrl = CreateWindowExW(0, TRACKBAR_CLASSW, L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   TBS_HORZ | TBS_NOTICKS,
                                   0, 0, 10, 10, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   AfxGetInstanceHandle(), nullptr);
        }
        if (ctrl)
        {
            SendMessage(ctrl, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
            SendMessage(ctrl, TBM_SETPOS, TRUE, value);
            SendMessageW(ctrl, WM_SETFONT, reinterpret_cast<WPARAM>(g_uiFont), TRUE);
            SetWindowTheme(ctrl, L"Explorer", nullptr);
        }
        return ctrl;
    }

    void SaveSoundSettings(SoundState* state)
    {
        if (!state || !state->secondary)
            return;

        int selection = static_cast<int>(SendMessage(state->secondary, CB_GETCURSEL, 0, 0));
        int secondary = PRO_SECONDARY_DISABLED;
        if (selection >= 0)
            secondary = static_cast<int>(SendMessage(state->secondary, CB_GETITEMDATA,
                                                     selection, 0));

        const int bass = static_cast<int>(SendMessage(state->bass, TBM_GETPOS, 0, 0));
        const int mid = static_cast<int>(SendMessage(state->mid, TBM_GETPOS, 0, 0));
        const int treble = static_cast<int>(SendMessage(state->treble, TBM_GETPOS, 0, 0));

        WriteProfileInt(L"SecondaryInputDevice", secondary);
        WriteProfileInt(L"EqualizerBass", bass);
        WriteProfileInt(L"EqualizerMid", mid);
        WriteProfileInt(L"EqualizerTreble", treble);

        if (ttInst)
        {
            TT_SetSoundInputEqualizer(ttInst, bass, mid, treble);
            TT_CloseSecondarySoundInputDevice(ttInst);
            if (secondary != PRO_SECONDARY_DISABLED &&
                secondary != TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL)
                TT_InitSecondarySoundInputDevice(ttInst, secondary);
        }
    }

    void LayoutSoundPage(HWND hwnd, SoundState* state)
    {
        RECT rc = {};
        GetClientRect(hwnd, &rc);
        const int width = rc.right - rc.left;
        const int height = rc.bottom - rc.top;
        if (width < 260 || height < 260 || !state)
            return;

        const int margin = 12;
        const int labelWidth = std::max(100, width / 3);
        const int fieldX = margin + labelWidth;
        const int fieldWidth = width - fieldX - margin;
        const int rowHeight = 24;

        const int eqHeadingY = height - 86;
        const int mediaY = eqHeadingY - 30;
        const int effectsY = mediaY - 52;
        const int buttonsY = effectsY - 32;
        const int secondaryY = buttonsY - 30;
        const int outputY = secondaryY - 30;
        const int inputY = outputY - 30;
        const int radioY = std::max(12, inputY - 30);

        MoveControl(hwnd, IDC_STATIC_GRPSOUNDSYSTEM, 4, 4, width - 8, height - 8);

        MoveControl(hwnd, IDC_RADIO_WASAPI, margin + 22, radioY,
                    (width - margin * 2) / 2 - 28, rowHeight);
        MoveControl(hwnd, IDC_RADIO_DIRECTSOUND, width / 2 + 8, radioY,
                    width / 2 - margin - 8, rowHeight);

        MoveControl(hwnd, IDC_STATIC_INPUT, margin, inputY, labelWidth - 4, rowHeight);
        MoveControl(hwnd, IDC_COMBO_INPUTDRIVER, fieldX, inputY - 2,
                    fieldWidth, 180);
        MoveControl(hwnd, IDC_STATIC_OUTPUT, margin, outputY, labelWidth - 4, rowHeight);
        MoveControl(hwnd, IDC_COMBO_OUTPUTDRIVER, fieldX, outputY - 2,
                    fieldWidth, 180);
        MoveControl(hwnd, IDC_NATIVE_SECONDARY_LABEL, margin, secondaryY,
                    labelWidth - 4, rowHeight);
        MoveControl(hwnd, IDC_NATIVE_SECONDARY_COMBO, fieldX, secondaryY - 2,
                    fieldWidth, 180);

        // Sample-rate text was useful in Classic but made the page dense. The
        // selected devices remain visible and the detailed rates can be exposed
        // later in device details, matching the cleaner Qt preference layout.
        MoveControl(hwnd, IDC_STATIC_INPUTPROP, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_INPUT_SAMPLERATES, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_OUTPUTPROP, 0, 0, 0, 0, false);
        MoveControl(hwnd, IDC_STATIC_OUTPUT_SAMPLERATES, 0, 0, 0, 0, false);

        const int buttonWidth = std::max(82, (width - margin * 2 - 16) / 3);
        MoveControl(hwnd, IDC_BUTTON_TEST, margin, buttonsY,
                    buttonWidth, rowHeight);
        MoveControl(hwnd, IDC_BUTTON_REFRESHSND, margin + buttonWidth + 8,
                    buttonsY, buttonWidth, rowHeight);
        MoveControl(hwnd, IDC_BUTTON_DEFAULT, margin + (buttonWidth + 8) * 2,
                    buttonsY, buttonWidth, rowHeight);

        const int effectGap = 8;
        const int effectWidth = (width - margin * 2 - effectGap) / 2;
        SetWindowTextW(GetDlgItem(hwnd, IDC_CHECK_ECHOCANCEL), L"Echo cancellation");
        SetWindowTextW(GetDlgItem(hwnd, IDC_CHECK_AGC), L"Automatic gain control");
        SetWindowTextW(GetDlgItem(hwnd, IDC_CHECK_DENOISE), L"Noise reduction");
        SetWindowTextW(GetDlgItem(hwnd, IDC_CHECK_POSITIONING), L"3D user positioning");
        MoveControl(hwnd, IDC_CHECK_ECHOCANCEL, margin, effectsY,
                    effectWidth, rowHeight);
        MoveControl(hwnd, IDC_CHECK_AGC, margin + effectWidth + effectGap,
                    effectsY, effectWidth, rowHeight);
        MoveControl(hwnd, IDC_CHECK_DENOISE, margin, effectsY + 26,
                    effectWidth, rowHeight);
        MoveControl(hwnd, IDC_CHECK_POSITIONING, margin + effectWidth + effectGap,
                    effectsY + 26, effectWidth, rowHeight);

        SetWindowTextW(GetDlgItem(hwnd, IDC_STATIC_MEDIASTREAMVOL), L"Media vs. voice volume");
        MoveControl(hwnd, IDC_STATIC_MEDIASTREAMVOL, margin, mediaY,
                    labelWidth, rowHeight);
        MoveControl(hwnd, IDC_SLIDER_MEDIASTREAM_VOL, fieldX, mediaY - 2,
                    fieldWidth, rowHeight + 4);

        MoveControl(hwnd, IDC_NATIVE_EQ_HEADING, margin, eqHeadingY,
                    width - margin * 2, 20);
        const int eqLabelWidth = 72;
        const int eqX = margin + eqLabelWidth;
        const int eqWidth = width - eqX - margin;
        const int eqFirst = eqHeadingY + 18;
        MoveControl(hwnd, IDC_NATIVE_EQ_BASS_LABEL, margin, eqFirst,
                    eqLabelWidth - 4, 20);
        MoveControl(hwnd, IDC_NATIVE_EQ_BASS, eqX, eqFirst,
                    eqWidth, 22);
        MoveControl(hwnd, IDC_NATIVE_EQ_MID_LABEL, margin, eqFirst + 22,
                    eqLabelWidth - 4, 20);
        MoveControl(hwnd, IDC_NATIVE_EQ_MID, eqX, eqFirst + 22,
                    eqWidth, 22);
        MoveControl(hwnd, IDC_NATIVE_EQ_TREBLE_LABEL, margin, eqFirst + 44,
                    eqLabelWidth - 4, 20);
        MoveControl(hwnd, IDC_NATIVE_EQ_TREBLE, eqX, eqFirst + 44,
                    eqWidth, 22);
    }

    void OrderSoundControls(HWND hwnd, SoundState* state)
    {
        if (!hwnd || !state)
            return;

        // NVDA associates an unlabeled native control with the static text that
        // immediately precedes it in dialog/Z order. The controls added by the
        // native Pro layer are created dynamically, so without an explicit order
        // all five labels appear first and "Agudos" can become the name of the
        // secondary-device combo while the sliders remain unnamed.
        HWND anchor = GetDlgItem(hwnd, IDC_COMBO_OUTPUTDRIVER);
        PlaceAfter(GetDlgItem(hwnd, IDC_NATIVE_SECONDARY_LABEL), anchor);
        PlaceAfter(state->secondary, anchor);

        // Preserve keyboard order according to the visual top-to-bottom layout.
        PlaceAfter(GetDlgItem(hwnd, IDC_BUTTON_TEST), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_BUTTON_REFRESHSND), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_BUTTON_DEFAULT), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_CHECK_ECHOCANCEL), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_CHECK_AGC), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_CHECK_DENOISE), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_CHECK_POSITIONING), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_STATIC_MEDIASTREAMVOL), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_SLIDER_MEDIASTREAM_VOL), anchor);

        PlaceAfter(GetDlgItem(hwnd, IDC_NATIVE_EQ_HEADING), anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_NATIVE_EQ_BASS_LABEL), anchor);
        PlaceAfter(state->bass, anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_NATIVE_EQ_MID_LABEL), anchor);
        PlaceAfter(state->mid, anchor);
        PlaceAfter(GetDlgItem(hwnd, IDC_NATIVE_EQ_TREBLE_LABEL), anchor);
        PlaceAfter(state->treble, anchor);
    }

    void ApplySoundPage(HWND hwnd)
    {
        SoundState* state = reinterpret_cast<SoundState*>(GetPropW(hwnd, SOUND_STATE_PROP));
        if (!state)
            return;

        ApplyFontToTree(hwnd);

        HWND group = GetDlgItem(hwnd, IDC_STATIC_GRPSOUNDSYSTEM);
        if (group)
            SetWindowTextW(group, L"Audio devices and microphone processing");

        // Keep these labels correct at the source. Accessibility hooks still set
        // explicit MSAA names as a fallback, but NVDA can also derive the names
        // directly from the adjacent static labels.
        EnsureStatic(hwnd, IDC_NATIVE_SECONDARY_LABEL,
                     _T("Microfone secundário"), false);
        EnsureStatic(hwnd, IDC_NATIVE_EQ_HEADING,
                     _T("Equalizador do microfone"), true);
        EnsureStatic(hwnd, IDC_NATIVE_EQ_BASS_LABEL, _T("Graves"), false);
        EnsureStatic(hwnd, IDC_NATIVE_EQ_MID_LABEL, _T("Médios"), false);
        EnsureStatic(hwnd, IDC_NATIVE_EQ_TREBLE_LABEL, _T("Agudos"), false);

        if (!state->secondary)
        {
            state->secondary = CreateWindowExW(0, L"COMBOBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                0, 0, 10, 100, hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_NATIVE_SECONDARY_COMBO)),
                AfxGetInstanceHandle(), nullptr);
            if (state->secondary)
            {
                SendMessageW(state->secondary, WM_SETFONT,
                             reinterpret_cast<WPARAM>(g_uiFont), TRUE);
                SetWindowTheme(state->secondary, L"Explorer", nullptr);
            }
        }

        FillSecondaryInput(state->secondary);
        state->bass = EnsureTrackbar(hwnd, IDC_NATIVE_EQ_BASS,
                                     ReadProfileInt(L"EqualizerBass", 0));
        state->mid = EnsureTrackbar(hwnd, IDC_NATIVE_EQ_MID,
                                    ReadProfileInt(L"EqualizerMid", 0));
        state->treble = EnsureTrackbar(hwnd, IDC_NATIVE_EQ_TREBLE,
                                       ReadProfileInt(L"EqualizerTreble", 0));

        OrderSoundControls(hwnd, state);

        // The inherited resource page is 221 dialog units tall. It normally
        // maps to enough pixels for this layout; if a DPI/font combination is
        // unusually compact, enlarge just the page and its property sheet.
        RECT page = {};
        GetClientRect(hwnd, &page);
        if (page.bottom - page.top < 320)
        {
            HWND sheet = GetParent(hwnd);
            RECT sheetRect = {};
            GetWindowRect(sheet, &sheetRect);
            SetWindowPos(sheet, nullptr, 0, 0,
                         sheetRect.right - sheetRect.left,
                         sheetRect.bottom - sheetRect.top + 120,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            RECT client = {};
            GetClientRect(sheet, &client);
            SetWindowPos(hwnd, nullptr, 0, 0,
                         page.right - page.left,
                         std::max<LONG>(320, client.bottom - 70),
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }

        LayoutSoundPage(hwnd, state);
        InvalidateRect(hwnd, nullptr, TRUE);
    }

    LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        MainState* state = reinterpret_cast<MainState*>(GetPropW(hwnd, MAIN_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_TT_APPLY_MODERN_UI)
        {
            ApplyMainWindow(hwnd);
            return 0;
        }

        if (msg == WM_GETMINMAXINFO && lParam)
        {
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            MINMAXINFO* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = std::max<LONG>(info->ptMinTrackSize.x, 760);
            info->ptMinTrackSize.y = std::max<LONG>(info->ptMinTrackSize.y, 500);
            return result;
        }

        if (msg == WM_SIZE)
        {
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            LayoutMainWindow(hwnd);
            return result;
        }

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, MAIN_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK SoundWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        SoundState* state = reinterpret_cast<SoundState*>(GetPropW(hwnd, SOUND_STATE_PROP));
        WNDPROC previous = state ? state->previous : nullptr;

        if (msg == WM_TT_APPLY_MODERN_UI)
        {
            ApplySoundPage(hwnd);
            return 0;
        }

        if (msg == WM_SIZE)
        {
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            LayoutSoundPage(hwnd, state);
            return result;
        }

        if (msg == WM_COMMAND && LOWORD(wParam) == IDC_BUTTON_REFRESHSND)
        {
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            FillSecondaryInput(state ? state->secondary : nullptr);
            return result;
        }

        if (msg == WM_NOTIFY && lParam)
        {
            NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
            if (hdr->code == PSN_APPLY)
                SaveSoundSettings(state);
        }

        if (msg == WM_NCDESTROY)
        {
            if (previous)
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(previous));
            RemovePropW(hwnd, SOUND_STATE_PROP);
            LRESULT result = previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                                      : DefWindowProcW(hwnd, msg, wParam, lParam);
            delete state;
            return result;
        }

        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void SubclassMain(HWND hwnd)
    {
        if (GetPropW(hwnd, MAIN_STATE_PROP))
            return;

        MainState* state = new MainState();
        state->previous = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(&MainWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }
        SetPropW(hwnd, MAIN_STATE_PROP, state);
        PostMessageW(hwnd, WM_TT_APPLY_MODERN_UI, 0, 0);
    }

    void SubclassSoundPage(HWND hwnd)
    {
        if (GetPropW(hwnd, SOUND_STATE_PROP))
            return;

        SoundState* state = new SoundState();
        state->previous = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(&SoundWndProc)));
        if (!state->previous)
        {
            delete state;
            return;
        }
        SetPropW(hwnd, SOUND_STATE_PROP, state);
        PostMessageW(hwnd, WM_TT_APPLY_MODERN_UI, 0, 0);
    }

    LRESULT CALLBACK CallWndProcHook(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code >= 0 && lParam)
        {
            const CWPSTRUCT* message = reinterpret_cast<const CWPSTRUCT*>(lParam);
            if (message->message == WM_INITDIALOG)
            {
                HWND hwnd = message->hwnd;
                if (GetDlgItem(hwnd, IDC_TREE_SESSION) &&
                    GetDlgItem(hwnd, IDC_TAB_CTRL))
                {
                    SubclassMain(hwnd);
                }
                else if (GetDlgItem(hwnd, IDC_COMBO_INPUTDRIVER) &&
                         GetDlgItem(hwnd, IDC_COMBO_OUTPUTDRIVER) &&
                         GetDlgItem(hwnd, IDC_CHECK_ECHOCANCEL))
                {
                    SubclassSoundPage(hwnd);
                }
            }
        }
        return CallNextHookEx(g_callWndHook, code, wParam, lParam);
    }
}

namespace NativeWindowsUI
{
    void Start()
    {
        if (g_callWndHook)
            return;

        g_callWndHook = SetWindowsHookExW(WH_CALLWNDPROC, CallWndProcHook,
                                          nullptr, GetCurrentThreadId());
    }

    void Stop()
    {
        if (g_callWndHook)
        {
            UnhookWindowsHookEx(g_callWndHook);
            g_callWndHook = nullptr;
        }

        if (g_headingFont)
        {
            DeleteObject(g_headingFont);
            g_headingFont = nullptr;
        }
        if (g_uiFont)
        {
            DeleteObject(g_uiFont);
            g_uiFont = nullptr;
        }
    }
}
