#include "stdafx.h"
#include "TypingHook.h"

#ifdef TT_GetMessage
#undef TT_GetMessage
#endif

#ifdef MyTextMessage
#undef MyTextMessage
#endif

#include <map>

namespace
{
    std::map<INT32, HWND> g_privateTypingDialogs;

    void DispatchTypingTextMessage(const TextMessage& textmsg)
    {
        if (textmsg.nMsgType != MSGTYPE_CUSTOM)
            return;

        CStringList tokens;
        GetCustomCommand(textmsg.szMessage, tokens);
        if (tokens.GetCount() < 2)
            return;

        POSITION pos = tokens.GetHeadPosition();
        CString command = tokens.GetNext(pos);
        if (command != TT_INTCMD_TYPING_TEXT)
            return;

        const bool typing = tokens.GetNext(pos) != _T("0");
        auto it = g_privateTypingDialogs.find(textmsg.nFromUserID);
        if (it == g_privateTypingDialogs.end())
            return;

        if (::IsWindow(it->second))
            ::PostMessage(it->second, WM_TT_REMOTE_TYPING, typing ? TRUE : FALSE, 0);
        else
            g_privateTypingDialogs.erase(it);
    }

    void DispatchTypingMessage(const TTMessage& msg)
    {
        if (msg.nClientEvent != CLIENTEVENT_CMD_USER_TEXTMSG)
            return;

        DispatchTypingTextMessage(msg.textmessage);
    }
}

void RegisterPrivateTypingDialog(INT32 nUserID, HWND hWnd)
{
    if (nUserID > 0 && ::IsWindow(hWnd))
        g_privateTypingDialogs[nUserID] = hWnd;
}

void UnregisterPrivateTypingDialog(INT32 nUserID, HWND hWnd)
{
    auto it = g_privateTypingDialogs.find(nUserID);
    if (it != g_privateTypingDialogs.end() && it->second == hWnd)
        g_privateTypingDialogs.erase(it);
}

MyTextMessage MyTextMessageWithTypingHook()
{
    return MyTextMessage();
}

MyTextMessage MyTextMessageWithTypingHook(const TextMessage& msg)
{
    MyTextMessage result(msg);
    DispatchTypingTextMessage(msg);
    return result;
}

TTBOOL TT_GetMessageWithTypingHook(TTInstance* lpTTInstance,
                                   TTMessage* pMsg,
                                   const INT32* pnWaitMs)
{
    TTBOOL result = TT_GetMessage(lpTTInstance, pMsg, pnWaitMs);
    if (result && pMsg)
        DispatchTypingMessage(*pMsg);
    return result;
}
