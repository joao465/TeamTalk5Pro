#pragma once

#define WM_TT_REMOTE_TYPING (WM_APP + 0x362)

TTBOOL TT_GetMessageWithTypingHook(IN TTInstance* lpTTInstance,
                                   OUT TTMessage* pMsg,
                                   IN const INT32* pnWaitMs);

void RegisterPrivateTypingDialog(INT32 nUserID, HWND hWnd);
void UnregisterPrivateTypingDialog(INT32 nUserID, HWND hWnd);

// Route conversions performed by CTeamTalkDlg::OnUserMessage through the
// typing dispatcher too. This mirrors the official client's behaviour of
// forwarding MSGTYPE_CUSTOM messages to the private-message session instead
// of relying only on the TT_GetMessage interception.
MyTextMessage MyTextMessageWithTypingHook();
MyTextMessage MyTextMessageWithTypingHook(const TextMessage& msg);

#define TT_GetMessage TT_GetMessageWithTypingHook
#define MyTextMessage(...) MyTextMessageWithTypingHook(__VA_ARGS__)
