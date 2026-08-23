#ifndef ClientDir
  #define ClientDir "C:\TeamTalkProClient"
#endif

#ifndef ProVersion
  #define ProVersion "5.26.2"
#endif

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
OutputBaseFilename=TeamTalk_5_Pro_{#ProVersion}_Setup_x64
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

function VCRedistExists(): Boolean;
begin
  Result := FileExists(ExpandConstant('{tmp}\vc_redist.x64.exe'));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  NeedsRestart := False;
  ImportOfficialConfig := False;

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
  if (CurStep = ssPostInstall) and ImportOfficialConfig then
  begin
    ForceDirectories(ExtractFileDir(ProConfigFile));

    if not CopyFile(OfficialConfigFile, ProConfigFile, False) then
      MsgBox(CustomMessage('ImportFailed'), mbError, MB_OK);
  end;
end;