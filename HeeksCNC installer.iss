; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{FE583F3D-8863-438B-AD81-790FD54A7113}
AppName=HeeksCNC
AppVerName=HeeksCNC 0.17.0
AppPublisher=Heeks Software
AppPublisherURL=http://heeks.net/
AppSupportURL=http://code.google.com/p/heekscnc/
AppUpdatesURL=http://code.google.com/p/heekscnc/
DefaultDirName={pf}\HeeksCNC
DefaultGroupName=HeeksCNC
DisableProgramGroupPage=yes
OutputBaseFilename=HeeksCNC 0.17.0
Compression=lzma
SolidCompression=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: de; MessagesFile: "compiler:Languages\German.isl"
Name: fr; MessagesFile: "compiler:Languages\French.isl"
Name: it; MessagesFile: "compiler:Languages\Italian.isl"

[Files]
Source: "C:\Users\Dan\HeeksCAD\src\Unicode Release\HeeksCAD.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\bitmaps\*.png"; DestDir: "{app}\bitmaps"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\bitmaps\font.glf"; DestDir: "{app}\bitmaps"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\icons\*.png"; DestDir: "{app}\icons"; Flags: ignoreversion
Source: "C:\Users\Dan\OCC dlls for HeeksCAD\*"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\de\*"; DestDir: "{app}\de"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\it\*"; DestDir: "{app}\it"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\fr\*"; DestDir: "{app}\fr"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\sk\*"; DestDir: "{app}\sk"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\wxmsw28u_gl_vc_custom.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\wxmsw28u_core_vc_custom.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\wxmsw28u_aui_vc_custom.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCAD\wxbase28u_vc_custom.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\post.bat"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\nc_read.bat"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\area_funcs.py"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\libarea\Release\area.pyd"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\kurve_funcs.py"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\area_funcs.py"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\area.pyd"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\ocl_funcs.py"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\ocl.pyd"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\*.speeds"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\*.tooltable"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\nc\*.py"; DestDir: "{app}\HeeksCNC\nc"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\nc\*.txt"; DestDir: "{app}\HeeksCNC\nc"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\src\Unicode Release\HeeksCNC.dll"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\bitmaps\*.png"; DestDir: "{app}\HeeksCNC\bitmaps"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\bitmaps\pocket\*.png"; DestDir: "{app}\HeeksCNC\bitmaps\pocket"; Flags: ignoreversion
Source: "C:\Users\Dan\HeeksCNC\icons\*.png"; DestDir: "{app}\HeeksCNC\icons"; Flags: ignoreversion
Source: "C:\Program Files\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.CRT\*"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Program Files\Microsoft Visual Studio 9.0\VC\redist\x86\Microsoft.VC90.CRT\*"; DestDir: "{app}\HeeksCNC"; Flags: ignoreversion
Source: "C:\apps\python-2.6.4.msi"; DestDir: "{tmp}"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\HeeksCNC"; Filename: "{app}\HeeksCAD.exe"; WorkingDir: "{app}"; Parameters: "HeeksCNC/HeeksCNC.dll"

[Run]
Filename: "{tmp}\python-2.6.4.msi"; Flags: shellexec skipifsilent
Filename: "{app}\HeeksCAD.exe"; WorkingDir: "{app}"; Parameters: "HeeksCNC/HeeksCNC.dll"; Description: "{cm:LaunchProgram,HeeksCNC}"; Flags: nowait postinstall skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\HeeksCAD"; Flags: uninsdeletekeyifempty


