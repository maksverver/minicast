;
; NSIS installation script
;

; Application description
Name "Minicast"
OutFile "release\Minicast-1.1.exe"
SetCompressor LZMA

; Installation settings
InstallDir "$PROGRAMFILES\Winamp"
InstallDirRegKey HKLM "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Winamp" "UninstallString"
DirText "Please select the Winamp directory, then click Install to start the installation."

; Uninstallation settings 
UninstallText "Please close Winamp before continuing!"

; Installer pages
Page directory
Page instfiles

; Uninstaller pages
UninstPage uninstConfirm
UninstPage instfiles


;
; Sections
;

Section "Install"
	SetOutPath "$INSTDIR"
	File "gogo.dll"
	WriteUninstaller "Uninstall Minicast.exe"
	SetOutPath "$INSTDIR\Plugins"
	File "release\dsp_minicast.dll"
	
	WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minicast" "DisplayName" "Minicast 1.1 for Winamp"
	WriteRegStr   HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minicast" "UninstallString" '"$INSTDIR\Uninstall Minicast.exe"'
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minicast" "NoModify" 1
	WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minicast" "NoRepair" 1
SectionEnd


Section "Uninstall"
	DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Minicast"
	DeleteRegKey HKCU "Software\Minicast"

	Delete "$INSTDIR\Plugins\dsp_minicast.dll"
	Delete "$INSTDIR\gogo.dll"
	Delete "$INSTDIR\Uninstall Minicast.exe"
SectionEnd


;
; End of script.
;
