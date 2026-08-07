[Setup]
AppName=LightCell
AppVersion=1.1.0
DefaultDirName={pf}\LightCell
DefaultGroupName=LightCell
UninstallDisplayIcon={app}\LightCell.exe
Compression=lzma2
SolidCompression=yes
OutputDir=Output
OutputBaseFilename=LightCell_Setup_v1.1.0

[Files]
Source: "LightCell_v1.1.0\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\LightCell"; Filename: "{app}\LightCell.exe"
Name: "{group}\Uninstall LightCell"; Filename: "{uninstallexe}"
Name: "{commondesktop}\LightCell"; Filename: "{app}\LightCell.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "바탕 화면에 바로 가기 만들기"; GroupDescription: "추가 아이콘"
