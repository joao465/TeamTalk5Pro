#ifndef ClientDir
  #define ClientDir "C:\TeamTalkProClient"
#endif

#ifndef ProVersion
  #define ProVersion "5.26.4.3"
#endif

#define InstallMusicFile "TeamTalkProInstall.mp3"
#define InstallMusicSHA256 "2a616f4e96db8bc01d84213759ee30e974e52cca944aa358a95b95823e46fb62"

[Setup]
AppId={{D62C0218-F591-4729-AA56-3769F9D2F61A}
AppName=TeamTalk 5 Pro
AppVersion={#ProVersion}
VersionInfoVersion={#ProVersion}.0
AppVerName=TeamTalk 5 (pro {#ProVersion})
AppPublisher=TeamTalk 5 Pro
DefaultDirName={autopf}\TeamTalk 5 Pro
DefaultGroupName=TeamTalk 5 Pro
UninstallDisplayName=TeamTalk 5 (pro {#ProVersion})
UninstallDisplayIcon={app}\TeamTalk5.exe
AllowNoIcons=yes
OutputBaseFilename=TeamTalk-Pro-{#ProVersion}-Setup-x64
SetupIconFile=..\..\..\Client\qtTeamTalk\images\teamtalk.ico
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
ChangesAssociations=no
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "pt_BR"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[CustomMessages]
en.ImportOfficialConfig=Official TeamTalk settings were found at "%1". Do you want to import them into TeamTalk 5 Pro? The official TeamTalk installation and settings will not be changed.
pt_BR.ImportOfficialConfig=Foram encontradas configurações do TeamTalk oficial em "%1". Deseja importar essas configurações para o TeamTalk 5 Pro? A instalação e as configurações do TeamTalk oficial não serão alteradas.
en.ImportFailed=TeamTalk 5 Pro was installed, but the official TeamTalk settings could not be imported.
pt_BR.ImportFailed=O TeamTalk 5 Pro foi instalado, mas não foi possível importar as configurações do TeamTalk oficial.

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#ClientDir}\*"; Excludes: "vc_redist.x64.exe"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ClientDir}\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall skipifsourcedoesntexist
Source: "Music\TeamTalkProInstall.mp3.b64.part01a"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part01b"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part02"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part03"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part04"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part05"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part06"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part07"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part08"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part09"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part10a"; Flags: dontcopy
Source: "Music\TeamTalkProInstall.mp3.b64.part10b"; Flags: dontcopy

[Icons]
Name: "{group}\TeamTalk 5 Pro"; Filename: "{app}\TeamTalk5.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\TeamTalk 5 Pro"; Filename: "{app}\TeamTalk5.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; Flags: runhidden waituntilterminated; Check: VCRedistExists
Filename: "{app}\TeamTalk5.exe"; Description: "{cm:LaunchProgram,TeamTalk 5 Pro}"; WorkingDir: "{app}"; Flags: postinstall nowait skipifsilent

[Code]
var
  ImportOfficialConfig: Boolean;
  OfficialConfigFile: String;
  ProConfigFile: String;
  InstallMusicPrepared: Boolean;

const
  InstallMusicAlias = 'TeamTalkProInstallMusic';

function mciSendString(lpstrCommand: String; lpstrReturnString: String;
  uReturnLength: Cardinal; hwndCallback: Integer): Integer;
  external 'mciSendStringW@winmm.dll stdcall';

procedure StopInstallMusic;
begin
  mciSendString('stop ' + InstallMusicAlias, '', 0, 0);
  mciSendString('close ' + InstallMusicAlias, '', 0, 0);
end;

procedure ExtractInstallMusicParts;
begin
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part01a');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part01b');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part02');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part03');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part04');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part05');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part06');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part07');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part08');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part09');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part10a');
  ExtractTemporaryFile('TeamTalkProInstall.mp3.b64.part10b');
end;

procedure DeleteInstallMusicParts;
begin
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part01a'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part01b'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part02'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part03'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part04'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part05'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part06'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part07'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part08'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part09'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part10a'));
  DeleteFile(ExpandConstant('{tmp}\TeamTalkProInstall.mp3.b64.part10b'));
end;

function PrepareInstallMusic: Boolean;
var
  MusicPath: String;
  PowerShellPath: String;
  ScriptPath: String;
  ScriptText: String;
  ResultCode: Integer;
begin
  Result := False;

  if InstallMusicPrepared then
  begin
    Result := FileExists(ExpandConstant('{tmp}\{#InstallMusicFile}'));
    Exit;
  end;

  InstallMusicPrepared := True;
  ExtractInstallMusicParts;

  MusicPath := ExpandConstant('{tmp}\{#InstallMusicFile}');
  ScriptPath := ExpandConstant('{tmp}\DecodeTeamTalkProInstallMusic.ps1');
  PowerShellPath := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');

  ScriptText :=
    '$parts = Get-ChildItem -LiteralPath "' + ExpandConstant('{tmp}') +
    '" -Filter "TeamTalkProInstall.mp3.b64.part*" | Sort-Object Name' + #13#10 +
    '$base64 = ($parts | ForEach-Object { (Get-Content -LiteralPath $_.FullName -Raw).Trim() }) -join ""' + #13#10 +
    '[IO.File]::WriteAllBytes("' + MusicPath +
    '", [Convert]::FromBase64String($base64))' + #13#10 +
    '$hash = (Get-FileHash -LiteralPath "' + MusicPath +
    '" -Algorithm SHA256).Hash.ToLowerInvariant()' + #13#10 +
    'if ($hash -ne "{#InstallMusicSHA256}") { Remove-Item -LiteralPath "' +
    MusicPath + '" -Force -ErrorAction SilentlyContinue; exit 2 }';

  if not SaveStringToFile(ScriptPath, ScriptText, False) then
  begin
    DeleteInstallMusicParts;
    Exit;
  end;

  if not Exec(PowerShellPath,
    '-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' + ScriptPath + '"',
    '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
  begin
    DeleteFile(ScriptPath);
    DeleteInstallMusicParts;
    Exit;
  end;

  DeleteFile(ScriptPath);
  DeleteInstallMusicParts;
  Result := (ResultCode = 0) and FileExists(MusicPath);
end;

procedure StartInstallMusic;
var
  MusicPath: String;
  OpenCommand: String;
begin
  if WizardSilent or (not PrepareInstallMusic) then
    Exit;

  MusicPath := ExpandConstant('{tmp}\{#InstallMusicFile}');
  StopInstallMusic;

  OpenCommand := 'open "' + MusicPath + '" type mpegvideo alias ' + InstallMusicAlias;
  if mciSendString(OpenCommand, '', 0, 0) = 0 then
  begin
    mciSendString('setaudio ' + InstallMusicAlias + ' volume to 700', '', 0, 0);
    mciSendString('play ' + InstallMusicAlias + ' repeat', '', 0, 0);
  end;
end;

function VCRedistExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{tmp}\vc_redist.x64.exe'));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  NeedsRestart := False;
  ImportOfficialConfig := False;
  InstallMusicPrepared := False;

  OfficialConfigFile := ExpandConstant('{userappdata}\BearWare.dk\TeamTalk5.ini');
  ProConfigFile := ExpandConstant('{userappdata}\TeamTalk 5 Pro\TeamTalk5Pro.ini');

  if FileExists(OfficialConfigFile) and
     (not FileExists(ProConfigFile)) and
     (not WizardSilent) then
  begin
    ImportOfficialConfig :=
      MsgBox(
        FmtMessage(CustomMessage('ImportOfficialConfig'), [OfficialConfigFile]),
        mbConfirmation,
        MB_YESNO
      ) = IDYES;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    StartInstallMusic
  else if CurStep = ssPostInstall then
    StopInstallMusic;

  if (CurStep = ssPostInstall) and ImportOfficialConfig then
  begin
    ForceDirectories(ExtractFileDir(ProConfigFile));

    if not CopyFile(OfficialConfigFile, ProConfigFile, False) then
      MsgBox(CustomMessage('ImportFailed'), mbError, MB_OK);
  end;
end;

procedure DeinitializeSetup;
begin
  StopInstallMusic;
  DeleteInstallMusicParts;
  DeleteFile(ExpandConstant('{tmp}\DecodeTeamTalkProInstallMusic.ps1'));
  DeleteFile(ExpandConstant('{tmp}\{#InstallMusicFile}'));
end;
