/*
 * TeamTalk 5 Pro native client application information.
 */
#pragma once

#define COMPANYNAME             _T("TeamTalk 5 Pro")
#define APPVERSION_SHORT        _T("5.26.5")
#define APPVERSION              _T( TEAMTALK_VERSION ) _T(" - Native")

#define APPTITLE                _T("TeamTalk 5 Pro v. ") APPVERSION
#define APPNAME                 _T("TeamTalk 5 Pro")
#define APPTITLE_SHORT          _T("TeamTalk5Pro")
#define SETTINGS_FILE           "TeamTalk5Pro.xml"
#define SETTINGS_DEFAULT_FILE   "TeamTalk5Pro.xml.default"

#define MANUALFILE              _T("TeamTalk5.chm")
#define WEBSITE                 _T("https://github.com/joao465/TeamTalk5Pro")
#define TEAMTALK_INSTALLDIR     _T("TeamTalk 5 Pro")
#define TTURL                   _T("tt://")
#define TT_XML_ROOTNAME         "teamtalk"
#define TTFILE_EXT              ".tt"

#define URL_PUBLICSERVER        _T("https://www.bearware.dk/teamtalk/tt5servers.php?client=") APPTITLE_SHORT _T("&version=") APPVERSION_SHORT _T("&dllversion=") _T( TEAMTALK_VERSION ) _T("&os=Windows")
// TeamTalk Pro updates are handled by ProNativeRuntime. This URL remains only
// for compatibility with the legacy update plumbing in the inherited MFC UI.
#define URL_APPUPDATE           _T("https://api.github.com/repos/joao465/TeamTalk5Pro/releases/latest")

#define WEBLOGIN_BEARWARE_USERNAME              "bearware"
#define WEBLOGIN_BEARWARE_USERNAMEPOSTFIX       "@bearware.dk"
#define WEBLOGIN_URL                            _T("https://www.bearware.dk/teamtalk/weblogin.php?client=") APPTITLE_SHORT _T("&version=") APPVERSION_SHORT _T("&dllversion=") _T( TEAMTALK_VERSION ) _T("&os=Windows")
#define WEBLOGIN_BEARWARE_URLAUTH(uid, passwd)  WEBLOGIN_URL _T("&service=bearware&action=auth&username=") + CString(uid) + _T("&password=") + CString(passwd)
#define WEBLOGIN_BEARWARE_URLTOKEN(uid, token, accesstoken)  WEBLOGIN_URL _T("&service=bearware&action=clientauth&username=") + CString(uid) + _T("&token=") + CString(token) + _T("&accesstoken=") + CString(accesstoken)
