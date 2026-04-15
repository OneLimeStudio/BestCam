[Setup]
AppName=WebcamBridge
AppVersion=1.0
DefaultDirName={autopf}\WebcamBridge
DefaultGroupName=WebcamBridge
OutputDir=Output
OutputBaseFilename=WebcamBridge_Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64
LicenseFile=license.txt

[Files]
Source: "dist\WebcamBridge\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "UnityCapture\*"; DestDir: "{app}\UnityCapture"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "usb_driver\*"; DestDir: "{app}\usb_driver"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\WebcamBridge"; Filename: "{app}\WebcamBridge.exe"
Name: "{group}\WebcamBridge"; Filename: "{app}\WebcamBridge.exe"
Name: "{group}\Uninstall WebcamBridge"; Filename: "{uninstallexe}"
Name: "{autostartup}\WebcamBridge"; Filename: "{app}\WebcamBridge.exe"

[Run]
; Install ADB USB Driver using pnputil
Filename: "{sys}\pnputil.exe"; Parameters: "/add-driver ""{app}\usb_driver\android_winusb.inf"" /install /force"; StatusMsg: "Installing Android USB Driver..."; Flags: runhidden waituntilterminated

; Install UnityCapture virtual camera (32-bit)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\UnityCapture\UnityCaptureFilter32.dll"""; StatusMsg: "Installing virtual camera driver..."; Flags: runhidden waituntilterminated

; Install UnityCapture virtual camera (64-bit)
Filename: "{sys}\regsvr32.exe"; Parameters: "/s ""{app}\UnityCapture\UnityCaptureFilter64.dll"""; StatusMsg: "Installing 64-bit virtual camera driver..."; Flags: runhidden waituntilterminated; Check: Is64BitInstallMode

; Launch app
Filename: "{app}\WebcamBridge.exe"; Description: "Launch WebcamBridge"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\UnityCapture\UnityCaptureFilter32.dll"""; Flags: runhidden
Filename: "{sys}\regsvr32.exe"; Parameters: "/u /s ""{app}\UnityCapture\UnityCaptureFilter64.dll"""; Flags: runhidden; Check: Is64BitInstallMode

