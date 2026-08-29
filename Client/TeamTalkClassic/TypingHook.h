#pragma once

#define WM_TT_REMOTE_TYPING (WM_APP + 0x362)

TTBOOL TT_GetMessageWithTypingHook(IN TTInstance* lpTTInstance,
                                   OUT TTMessage* pMsg,
                                   IN const INT32* pnWaitMs);

void RegisterPrivateTypingDialog(INT32 nUserID, HWND hWnd);
void UnregisterPrivateTypingDialog(INT32 nUserID, HWND hWnd);

#define TT_GetMessage TT_GetMessageWithTypingHook
