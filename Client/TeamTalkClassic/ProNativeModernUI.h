#pragma once

class CTeamTalkDlg;

namespace ProNativeModernUI
{
    // Adds the TeamTalk Pro menu and applies current Windows native theming
    // without introducing Qt or another GUI framework.
    void Start(CTeamTalkDlg* dialog);
    void Stop();
}
