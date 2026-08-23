#include "stdafx.h"
#include "ProNativeRuntime.h"
#include "AppInfo.h"
#include "Helper.h"
#include "TeamTalkDlg.h"
#include "settings/ClientXML.h"

#include <ShlObj.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <shellapi.h>
#include <urlmon.h>
#include <winhttp.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "winhttp.lib")

extern TTInstance* ttInst;

namespace
{
    constexpr int PRO_SECONDARY_DISABLED = -32768;
    constexpr wchar_t PRO_REPOSITORY[] = L"joao465/TeamTalk5Pro";

    std::atomic<bool> g_running{ false };
    std::thread g_worker;
    CTeamTalkDlg* g_dialog = nullptr;

    struct ProAudioSettings
    {
        int secondaryInput = PRO_SECONDARY_DISABLED;
        int bass = 0;
        int mid = 0;
        int treble = 0;
        bool updates = true;
    };

    std::wstring Utf8ToWide(const std::string& input)
    {
        if (input.empty()) return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
        if (size <= 0) return std::wstring();
        std::wstring output(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, output.data(), size);
        if (!output.empty() && output.back() == L'\0') output.pop_back();
        return output;
    }

    std::string WideToUtf8(const std::wstring& input)
    {
        if (input.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return std::string();
        std::string output(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, output.data(), size, nullptr, nullptr);
        if (!output.empty() && output.back() == '\0') output.pop_back();
        return output;
    }

    std::string WideToLocal(const std::wstring& input)
    {
        if (input.empty()) return std::string();
        int size = WideCharToMultiByte(CP_ACP, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 0) return std::string();
        std::string output(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_ACP, 0, input.c_str(), -1, output.data(), size, nullptr, nullptr);
        if (!output.empty() && output.back() == '\0') output.pop_back();
        return output;
    }

    std::wstring GetProFolder()
    {
        wchar_t appdata[MAX_PATH] = {};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appdata)))
            return L".";
        std::wstring folder = appdata;
        folder += L"\\TeamTalk 5 Pro";
        CreateDirectoryW(folder.c_str(), nullptr);
        return folder;
    }

    std::wstring QtIniPath() { return GetProFolder() + L"\\TeamTalk5Pro.ini"; }
    std::wstring NativeXmlPath() { return GetProFolder() + L"\\TeamTalk5Pro.xml"; }
    std::wstring NativeProfilePath() { return GetProFolder() + L"\\TeamTalkProNative.ini"; }

    std::string Trim(std::string value)
    {
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
        while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
        return value;
    }

    std::string DecodeIniValue(std::string value)
    {
        value = Trim(value);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
        return value;
    }

    using IniMap = std::map<std::string, std::string>;

    IniMap ReadQtIni()
    {
        IniMap values;
        std::ifstream input(WideToLocal(QtIniPath()), std::ios::binary);
        if (!input) return values;

        std::string section;
        std::string line;
        bool first = true;
        while (std::getline(input, line))
        {
            if (first)
            {
                first = false;
                if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
                    line.erase(0, 3);
            }
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']')
            {
                section = line.substr(1, line.size() - 2);
                continue;
            }
            const auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            const std::string key = Trim(line.substr(0, pos));
            const std::string value = DecodeIniValue(line.substr(pos + 1));
            values[(section.empty() ? key : section + "/" + key)] = value;
        }
        return values;
    }

    bool Has(const IniMap& ini, const std::string& key) { return ini.find(key) != ini.end(); }
    std::string Get(const IniMap& ini, const std::string& key, const std::string& def = std::string())
    {
        auto it = ini.find(key);
        return it == ini.end() ? def : it->second;
    }
    int GetInt(const IniMap& ini, const std::string& key, int def)
    {
        auto it = ini.find(key);
        if (it == ini.end()) return def;
        try { return std::stoi(it->second); } catch (...) { return def; }
    }
    bool GetBool(const IniMap& ini, const std::string& key, bool def)
    {
        auto it = ini.find(key);
        if (it == ini.end()) return def;
        std::string v = it->second;
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (v == "true" || v == "1" || v == "yes") return true;
        if (v == "false" || v == "0" || v == "no") return false;
        return def;
    }

    ProAudioSettings LoadProAudioSettings()
    {
        ProAudioSettings s;
        const std::wstring path = NativeProfilePath();
        s.secondaryInput = static_cast<int>(GetPrivateProfileIntW(L"Audio", L"SecondaryInputDevice", PRO_SECONDARY_DISABLED, path.c_str()));
        s.bass = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Audio", L"EqualizerBass", 0, path.c_str())), 0, 100);
        s.mid = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Audio", L"EqualizerMid", 0, path.c_str())), 0, 100);
        s.treble = std::clamp(static_cast<int>(GetPrivateProfileIntW(L"Audio", L"EqualizerTreble", 0, path.c_str())), 0, 100);
        s.updates = GetPrivateProfileIntW(L"Updates", L"Enabled", 1, path.c_str()) != 0;
        return s;
    }

    void SaveProAudioSettings(const ProAudioSettings& s)
    {
        const std::wstring path = NativeProfilePath();
        WritePrivateProfileStringW(L"Audio", L"SecondaryInputDevice", std::to_wstring(s.secondaryInput).c_str(), path.c_str());
        WritePrivateProfileStringW(L"Audio", L"EqualizerBass", std::to_wstring(s.bass).c_str(), path.c_str());
        WritePrivateProfileStringW(L"Audio", L"EqualizerMid", std::to_wstring(s.mid).c_str(), path.c_str());
        WritePrivateProfileStringW(L"Audio", L"EqualizerTreble", std::to_wstring(s.treble).c_str(), path.c_str());
        WritePrivateProfileStringW(L"Updates", L"Enabled", s.updates ? L"1" : L"0", path.c_str());
    }

    void ApplyProAudio(const ProAudioSettings& s)
    {
        if (!ttInst) return;
        if (!(TT_GetFlags(ttInst) & CLIENT_SNDINPUT_READY)) return;

        TT_SetSoundInputEqualizer(ttInst, s.bass, s.mid, s.treble);
        TT_CloseSecondarySoundInputDevice(ttInst);
        if (s.secondaryInput != PRO_SECONDARY_DISABLED &&
            s.secondaryInput != TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL)
        {
            TT_InitSecondarySoundInputDevice(ttInst, s.secondaryInput);
        }
    }

    std::string HttpGet(const std::wstring& host, const std::wstring& path)
    {
        std::string result;
        HINTERNET session = WinHttpOpen(L"TeamTalk-5-Pro-Native/5.26.5", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (!session) return result;
        HINTERNET connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!connection) { WinHttpCloseHandle(session); return result; }
        HINTERNET request = WinHttpOpenRequest(connection, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!request) { WinHttpCloseHandle(connection); WinHttpCloseHandle(session); return result; }

        const wchar_t* headers = L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        BOOL ok = WinHttpSendRequest(request, headers, static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                  WinHttpReceiveResponse(request, nullptr);
        if (ok)
        {
            DWORD available = 0;
            do
            {
                available = 0;
                if (!WinHttpQueryDataAvailable(request, &available) || !available) break;
                std::vector<char> buffer(available);
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), available, &read)) break;
                result.append(buffer.data(), buffer.data() + read);
            } while (available > 0);
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return result;
    }

    std::string JsonStringAfter(const std::string& json, const std::string& key, size_t start = 0, size_t* foundAt = nullptr)
    {
        const std::string needle = "\"" + key + "\"";
        size_t p = json.find(needle, start);
        if (p == std::string::npos) return std::string();
        p = json.find(':', p + needle.size());
        if (p == std::string::npos) return std::string();
        p = json.find('"', p + 1);
        if (p == std::string::npos) return std::string();
        ++p;
        std::string out;
        bool escape = false;
        for (; p < json.size(); ++p)
        {
            char c = json[p];
            if (escape)
            {
                if (c == 'n') out.push_back('\n');
                else if (c == 'r') out.push_back('\r');
                else if (c == 't') out.push_back('\t');
                else out.push_back(c);
                escape = false;
            }
            else if (c == '\\') escape = true;
            else if (c == '"')
            {
                if (foundAt) *foundAt = p + 1;
                return out;
            }
            else out.push_back(c);
        }
        return std::string();
    }

    std::string NormalizeVersion(const std::string& text)
    {
        std::string v;
        bool started = false;
        for (char c : text)
        {
            if (std::isdigit(static_cast<unsigned char>(c))) { v.push_back(c); started = true; }
            else if (started && c == '.') v.push_back(c);
            else if (started) break;
        }
        while (!v.empty() && v.back() == '.') v.pop_back();
        return v;
    }

    bool Sha256File(const std::wstring& filename, std::string& hex)
    {
        bool success = false;
        BCRYPT_ALG_HANDLE alg = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        DWORD objectLen = 0, hashLen = 0, cb = 0;
        std::vector<UCHAR> object;
        std::vector<UCHAR> digest;
        HANDLE file = INVALID_HANDLE_VALUE;

        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) goto cleanup;
        if (BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLen), sizeof(objectLen), &cb, 0) != 0) goto cleanup;
        if (BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLen), sizeof(hashLen), &cb, 0) != 0) goto cleanup;
        object.resize(objectLen);
        digest.resize(hashLen);
        if (BCryptCreateHash(alg, &hash, object.data(), objectLen, nullptr, 0, 0) != 0) goto cleanup;

        file = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) goto cleanup;
        {
            std::vector<UCHAR> buffer(64 * 1024);
            DWORD read = 0;
            while (ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read)
                if (BCryptHashData(hash, buffer.data(), read, 0) != 0) goto cleanup;
        }
        if (BCryptFinishHash(hash, digest.data(), hashLen, 0) != 0) goto cleanup;
        {
            static const char digits[] = "0123456789abcdef";
            hex.clear();
            for (UCHAR b : digest) { hex.push_back(digits[b >> 4]); hex.push_back(digits[b & 15]); }
        }
        success = true;

    cleanup:
        if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
        if (hash) BCryptDestroyHash(hash);
        if (alg) BCryptCloseAlgorithmProvider(alg, 0);
        return success;
    }

    void CheckForUpdates(HWND owner)
    {
        ProAudioSettings settings = LoadProAudioSettings();
        if (!settings.updates) return;

        const std::string json = HttpGet(L"api.github.com", L"/repos/joao465/TeamTalk5Pro/releases/latest");
        if (json.empty()) return;

        const std::string tag = JsonStringAfter(json, "tag_name");
        const std::string version = NormalizeVersion(tag);
        if (version.empty()) return;

        const CString current(APPVERSION_SHORT);
        const CString available(Utf8ToWide(version).c_str());
        if (VersionSameOrLater(current, available)) return;

        size_t assetsPos = json.find("\"assets\"");
        if (assetsPos == std::string::npos) return;
        std::string assetName, assetUrl, digest;
        size_t cursor = assetsPos;
        while (cursor < json.size())
        {
            size_t next = cursor;
            std::string name = JsonStringAfter(json, "name", cursor, &next);
            if (name.empty()) break;
            cursor = next;
            if (name.find("TeamTalk_5_Pro_") != std::string::npos && name.size() > 4 && name.substr(name.size() - 4) == ".exe")
            {
                assetName = name;
                assetUrl = JsonStringAfter(json, "browser_download_url", cursor);
                digest = JsonStringAfter(json, "digest", cursor);
                break;
            }
        }
        if (assetUrl.empty()) return;

        std::wostringstream prompt;
        prompt << L"Uma nova versão do TeamTalk 5 Pro está disponível.\n\nVersão atual: " << APPVERSION_SHORT
               << L"\nNova versão: " << Utf8ToWide(version) << L"\n\nDeseja atualizar agora?";
        if (MessageBoxW(owner, prompt.str().c_str(), L"Atualização do TeamTalk 5 Pro", MB_ICONQUESTION | MB_YESNO) != IDYES)
            return;

        wchar_t tempPath[MAX_PATH] = {};
        GetTempPathW(MAX_PATH, tempPath);
        std::wstring installer = tempPath;
        installer += Utf8ToWide(assetName);
        if (FAILED(URLDownloadToFileW(nullptr, Utf8ToWide(assetUrl).c_str(), installer.c_str(), 0, nullptr)))
        {
            MessageBoxW(owner, L"Não foi possível baixar a atualização.", L"TeamTalk 5 Pro", MB_ICONERROR | MB_OK);
            return;
        }

        if (digest.rfind("sha256:", 0) == 0)
        {
            std::string actual;
            std::string expected = digest.substr(7);
            std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!Sha256File(installer, actual) || actual != expected)
            {
                DeleteFileW(installer.c_str());
                MessageBoxW(owner, L"A verificação de integridade da atualização falhou.", L"TeamTalk 5 Pro", MB_ICONERROR | MB_OK);
                return;
            }
        }

        HINSTANCE result = ShellExecuteW(owner, L"open", installer.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) > 32 && owner)
            PostMessageW(owner, WM_CLOSE, 0, 0);
    }

    void MigrateHostEntries(const IniMap& ini, teamtalk::ClientXML& xml, bool latest)
    {
        const std::string prefix = latest ? "latesthosts/" : "serverentries/";
        for (int i = 0; i < 100; ++i)
        {
            const std::string p = prefix + std::to_string(i) + "_";
            const std::string address = Get(ini, p + "hostaddr");
            if (address.empty()) break;
            teamtalk::HostEntry host;
            host.szEntryName = Get(ini, p + "name", address);
            host.szAddress = address;
            host.nTcpPort = GetInt(ini, p + "tcpport", 10333);
            host.nUdpPort = GetInt(ini, p + "udpport", 10333);
            host.bEncrypted = GetBool(ini, p + "encrypted", false);
            host.szUsername = Get(ini, p + "username");
            host.szPassword = Get(ini, p + "password");
            host.szNickname = Get(ini, p + "nickname");
            host.szChannel = Get(ini, p + "channel");
            host.szChPasswd = Get(ini, p + "chanpassword");
            if (latest) xml.AddLatestHostEntry(host);
            else xml.AddHostManagerEntry(host);
        }
    }

    void ImportQtIni(const IniMap& ini)
    {
        if (ini.empty() || GetFileAttributesW(NativeXmlPath().c_str()) != INVALID_FILE_ATTRIBUTES) return;

        teamtalk::ClientXML xml(TT_XML_ROOTNAME);
        if (!xml.CreateFile(WideToLocal(NativeXmlPath()))) return;

        if (Has(ini, "general_/nickname")) xml.SetNickname(Get(ini, "general_/nickname"));
        if (Has(ini, "general_/statusmsg")) xml.SetStatusMessage(Get(ini, "general_/statusmsg"));
        if (Has(ini, "general_/gender")) xml.SetGender(GetInt(ini, "general_/gender", GENDER_NEUTRAL));
        if (Has(ini, "general_/voice-activated")) xml.SetVoiceActivated(GetBool(ini, "general_/voice-activated", false));
        if (Has(ini, "general_/auto-away")) xml.SetInactivityDelay(GetInt(ini, "general_/auto-away", 180));

        if (Has(ini, "display/startminimized")) xml.SetStartMinimized(GetBool(ini, "display/startminimized", false));
        if (Has(ini, "display/trayminimize")) xml.SetMinimizeToTray(GetBool(ini, "display/trayminimize", false));
        if (Has(ini, "display/alwaysontop")) xml.SetAlwaysOnTop(GetBool(ini, "display/alwaysontop", false));
        if (Has(ini, "display/userscount")) xml.SetShowUserCount(GetBool(ini, "display/userscount", true));
        if (Has(ini, "display/showusername")) xml.SetShowUsernames(GetBool(ini, "display/showusername", false));
        if (Has(ini, "display/disable-message-timestamp"))
            xml.SetMessageTimeStamp(!GetBool(ini, "display/disable-message-timestamp", false));

        if (Has(ini, "connection/autoconnect")) xml.SetAutoConnectToLastest(GetBool(ini, "connection/autoconnect", false));
        if (Has(ini, "connection/reconnect")) xml.SetReconnectOnDropped(GetBool(ini, "connection/reconnect", true));
        if (Has(ini, "connection/autojoin")) xml.SetAutoJoinRootChannel(GetBool(ini, "connection/autojoin", true));
        if (Has(ini, "connection/localtcpport")) xml.SetClientTcpPort(GetInt(ini, "connection/localtcpport", 0));
        if (Has(ini, "connection/localudpport")) xml.SetClientUdpPort(GetInt(ini, "connection/localudpport", 0));

        if (Has(ini, "soundsystem/inputdeviceid")) xml.SetSoundInputDevice(GetInt(ini, "soundsystem/inputdeviceid", UNDEFINED));
        if (Has(ini, "soundsystem/inputdeviceuid")) xml.SetSoundInputDevice(Get(ini, "soundsystem/inputdeviceuid"));
        if (Has(ini, "soundsystem/outputdeviceid")) xml.SetSoundOutputDevice(GetInt(ini, "soundsystem/outputdeviceid", UNDEFINED));
        if (Has(ini, "soundsystem/outputdeviceuid")) xml.SetSoundOutputDevice(Get(ini, "soundsystem/outputdeviceuid"));
        if (Has(ini, "soundsystem/mastervolume")) xml.SetSoundOutputVolume(GetInt(ini, "soundsystem/mastervolume", 50));
        if (Has(ini, "soundsystem/mediastream")) xml.SetMediaStreamVsVoice(GetInt(ini, "soundsystem/mediastream", 100));
        if (Has(ini, "soundsystem/voice-activation-level")) xml.SetVoiceActivationLevel(GetInt(ini, "soundsystem/voice-activation-level", 2));
        if (Has(ini, "soundsystem/echocancellation")) xml.SetEchoCancel(GetBool(ini, "soundsystem/echocancellation", false));
        if (Has(ini, "soundsystem/agc")) xml.SetAGC(GetBool(ini, "soundsystem/agc", false));
        if (Has(ini, "soundsystem/denoising")) xml.SetDenoise(GetBool(ini, "soundsystem/denoising", false));

        // The BearWare Classic updater is intentionally disabled. ProNativeRuntime owns updates.
        xml.SetCheckApplicationUpdates(false);
        MigrateHostEntries(ini, xml, false);
        MigrateHostEntries(ini, xml, true);
        xml.SaveFile();
    }

    void ImportProExtras(const IniMap& ini)
    {
        const std::wstring profile = NativeProfilePath();
        if (GetFileAttributesW(profile.c_str()) != INVALID_FILE_ATTRIBUTES) return;
        ProAudioSettings settings;
        settings.secondaryInput = GetInt(ini, "soundsystem/secondary-inputdeviceid", PRO_SECONDARY_DISABLED);
        settings.bass = std::clamp(GetInt(ini, "soundsystem/microphone-eq-bass", 0), 0, 100);
        settings.mid = std::clamp(GetInt(ini, "soundsystem/microphone-eq-mid", 0), 0, 100);
        settings.treble = std::clamp(GetInt(ini, "soundsystem/microphone-eq-treble", 0), 0, 100);
        settings.updates = GetBool(ini, "display/check-appupdate", true);
        SaveProAudioSettings(settings);
    }

    struct AudioWindowState
    {
        HWND hwnd = nullptr;
        HWND secondary = nullptr;
        HWND bass = nullptr;
        HWND mid = nullptr;
        HWND treble = nullptr;
        HWND updateCheck = nullptr;
        std::vector<SoundDevice> devices;
        ProAudioSettings settings;
        bool done = false;
    };

    constexpr int IDC_SECONDARY = 5001;
    constexpr int IDC_BASS = 5002;
    constexpr int IDC_MID = 5003;
    constexpr int IDC_TREBLE = 5004;
    constexpr int IDC_UPDATES = 5005;
    constexpr int IDC_SAVE = 5006;
    constexpr int IDC_CANCEL_PRO = 5007;

    void AddLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h)
    {
        CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    }

    LRESULT CALLBACK AudioWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto* state = reinterpret_cast<AudioWindowState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (msg == WM_NCCREATE)
        {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<AudioWindowState*>(cs->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            state->hwnd = hwnd;
        }
        if (!state) return DefWindowProcW(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_CREATE:
        {
            HFONT font = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            AddLabel(hwnd, L"Entrada de áudio secundária", 16, 16, 190, 20);
            state->secondary = CreateWindowExW(WS_EX_CLIENTEDGE, WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                                               16, 38, 430, 240, hwnd, reinterpret_cast<HMENU>(IDC_SECONDARY), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->secondary, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            int index = static_cast<int>(SendMessageW(state->secondary, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Desativada")));
            SendMessageW(state->secondary, CB_SETITEMDATA, index, PRO_SECONDARY_DISABLED);
            SendMessageW(state->secondary, CB_SETCURSEL, 0, 0);

            INT32 count = 0;
            TT_GetSoundDevices(nullptr, &count);
            if (count > 0)
            {
                state->devices.resize(count);
                if (TT_GetSoundDevices(state->devices.data(), &count))
                {
                    for (int i = 0; i < count; ++i)
                    {
                        const SoundDevice& d = state->devices[i];
                        if (d.nMaxInputChannels <= 0 || d.nDeviceID == TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL) continue;
                        int item = static_cast<int>(SendMessageW(state->secondary, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(d.szDeviceName)));
                        SendMessageW(state->secondary, CB_SETITEMDATA, item, d.nDeviceID);
                        if (d.nDeviceID == state->settings.secondaryInput) SendMessageW(state->secondary, CB_SETCURSEL, item, 0);
                    }
                }
            }

            AddLabel(hwnd, L"Graves", 16, 82, 80, 20);
            state->bass = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS,
                                          100, 78, 346, 34, hwnd, reinterpret_cast<HMENU>(IDC_BASS), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->bass, TBM_SETRANGE, TRUE, MAKELONG(0, 100)); SendMessageW(state->bass, TBM_SETPOS, TRUE, state->settings.bass);
            AddLabel(hwnd, L"Médios", 16, 126, 80, 20);
            state->mid = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS,
                                         100, 122, 346, 34, hwnd, reinterpret_cast<HMENU>(IDC_MID), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->mid, TBM_SETRANGE, TRUE, MAKELONG(0, 100)); SendMessageW(state->mid, TBM_SETPOS, TRUE, state->settings.mid);
            AddLabel(hwnd, L"Agudos", 16, 170, 80, 20);
            state->treble = CreateWindowExW(0, TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_AUTOTICKS,
                                            100, 166, 346, 34, hwnd, reinterpret_cast<HMENU>(IDC_TREBLE), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->treble, TBM_SETRANGE, TRUE, MAKELONG(0, 100)); SendMessageW(state->treble, TBM_SETPOS, TRUE, state->settings.treble);

            state->updateCheck = CreateWindowExW(0, L"BUTTON", L"Procurar atualizações automaticamente", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                                 16, 214, 300, 24, hwnd, reinterpret_cast<HMENU>(IDC_UPDATES), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(state->updateCheck, BM_SETCHECK, state->settings.updates ? BST_CHECKED : BST_UNCHECKED, 0);
            SendMessageW(state->updateCheck, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

            HWND save = CreateWindowExW(0, L"BUTTON", L"&Salvar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                        286, 258, 76, 28, hwnd, reinterpret_cast<HMENU>(IDC_SAVE), GetModuleHandleW(nullptr), nullptr);
            HWND cancel = CreateWindowExW(0, L"BUTTON", L"&Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                          370, 258, 76, 28, hwnd, reinterpret_cast<HMENU>(IDC_CANCEL_PRO), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(save, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE); SendMessageW(cancel, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SAVE)
            {
                int selected = static_cast<int>(SendMessageW(state->secondary, CB_GETCURSEL, 0, 0));
                if (selected != CB_ERR) state->settings.secondaryInput = static_cast<int>(SendMessageW(state->secondary, CB_GETITEMDATA, selected, 0));
                state->settings.bass = static_cast<int>(SendMessageW(state->bass, TBM_GETPOS, 0, 0));
                state->settings.mid = static_cast<int>(SendMessageW(state->mid, TBM_GETPOS, 0, 0));
                state->settings.treble = static_cast<int>(SendMessageW(state->treble, TBM_GETPOS, 0, 0));
                state->settings.updates = SendMessageW(state->updateCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
                SaveProAudioSettings(state->settings);
                ApplyProAudio(state->settings);
                DestroyWindow(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == IDC_CANCEL_PRO) { DestroyWindow(hwnd); return 0; }
            break;
        case WM_CLOSE: DestroyWindow(hwnd); return 0;
        case WM_DESTROY: state->done = true; return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void WorkerMain()
    {
        HWND owner = nullptr;
        for (int i = 0; g_running && i < 100; ++i)
        {
            if (g_dialog && ::IsWindow(g_dialog->GetSafeHwnd())) { owner = g_dialog->GetSafeHwnd(); break; }
            Sleep(100);
        }

        if (g_running)
        {
            Sleep(1500);
            if (g_running) CheckForUpdates(owner);
        }

        FILETIME lastWrite = {};
        int ticks = 0;
        while (g_running)
        {
            WIN32_FILE_ATTRIBUTE_DATA data = {};
            bool changed = false;
            if (GetFileAttributesExW(NativeProfilePath().c_str(), GetFileExInfoStandard, &data))
            {
                if (CompareFileTime(&lastWrite, &data.ftLastWriteTime) != 0)
                {
                    lastWrite = data.ftLastWriteTime;
                    changed = true;
                }
            }
            if (changed || ticks % 20 == 0) ApplyProAudio(LoadProAudioSettings());
            ++ticks;
            for (int i = 0; g_running && i < 10; ++i) Sleep(100);
        }
    }
}

namespace ProNativeRuntime
{
    void MigrateQtSettings()
    {
        const IniMap ini = ReadQtIni();
        ImportProExtras(ini);
        ImportQtIni(ini);
    }

    void Start(CTeamTalkDlg* dialog)
    {
        if (g_running.exchange(true)) return;
        g_dialog = dialog;
        g_worker = std::thread(WorkerMain);
    }

    void Stop()
    {
        if (!g_running.exchange(false)) return;
        if (g_worker.joinable()) g_worker.join();
        g_dialog = nullptr;
    }

    void ShowAudioSettings(HWND owner)
    {
        INITCOMMONCONTROLSEX cc = { sizeof(cc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
        InitCommonControlsEx(&cc);

        const wchar_t* cls = L"TeamTalkProNativeAudioSettings";
        WNDCLASSEXW wc = { sizeof(wc) };
        wc.lpfnWndProc = AudioWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);

        AudioWindowState state;
        state.settings = LoadProAudioSettings();
        HWND hwnd = CreateWindowExW(WS_EX_APPWINDOW, cls, L"TeamTalk 5 Pro - Áudio Pro",
                                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                    CW_USEDEFAULT, CW_USEDEFAULT, 480, 340, owner, nullptr, wc.hInstance, &state);
        if (!hwnd) return;
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);

        MSG msg;
        while (!state.done && GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            if (!IsDialogMessageW(hwnd, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        }
    }
}
