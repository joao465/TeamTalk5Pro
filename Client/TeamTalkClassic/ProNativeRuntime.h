#pragma once

class CTeamTalkDlg;

namespace ProNativeRuntime
{
    // Imports the existing TeamTalk Pro Qt INI into the native XML format
    // the first time the native client is started.
    void MigrateQtSettings();

    // Starts/stops the native background services (auto update and Pro audio).
    void Start(CTeamTalkDlg* dialog);
    void Stop();

    // Native audio configuration window used by the Start Menu shortcut.
    void ShowAudioSettings(HWND owner);
}
