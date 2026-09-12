#ifndef BuildTag
#define BuildTag "dev"
#endif
#define Root "..\.."
#define Binaries Root + "\build\windows-x64\SPAGlitch_artefacts\Release"
[Setup]
AppId={{68D7F995-3813-48B5-A7B4-3154D7D6067D}
AppName=SPAGlitch
AppVersion=0.1.3
AppPublisher=Silverplatter Audio
DefaultDirName={autopf}\Silverplatter Audio\SPAGlitch
DefaultGroupName=Silverplatter Audio
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#Root}\build\installer
OutputBaseFilename=SPAGlitch-{#BuildTag}-Windows-x64-Setup
SetupIconFile={#Root}\Assets\branding\app-icon.ico
UninstallDisplayIcon={app}\SPAGlitch.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
InfoBeforeFile=README.txt
[Types]
Name: "full"; Description: "Standalone and VST3"
Name: "custom"; Description: "Custom installation"; Flags: iscustom
[Components]
Name: "standalone"; Description: "Standalone instrument"; Types: full
Name: "vst3"; Description: "VST3 plugin"; Types: full
[Files]
Source: "{#Root}\local\factory-samples\*.wav"; DestDir: "{commonappdata}\Silverplatter Audio\SPAGlitch\Samples"; Flags: ignoreversion
Source: "{#Binaries}\Standalone\SPAGlitch.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "{#Binaries}\VST3\SPAGlitch.vst3\*"; DestDir: "{commoncf64}\VST3\SPAGlitch.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README.txt"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{group}\SPAGlitch"; Filename: "{app}\SPAGlitch.exe"; Components: standalone
