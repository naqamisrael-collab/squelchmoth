; Squelchmoth for Windows - installer (NSIS 3, built on Linux by scripts/package_windows.sh)
; Installs to Program Files\Squelchmoth, adds Start menu and desktop shortcuts, registers an uninstaller with
; Windows Settings > Apps. Sessions in Documents\Squelchmoth Sessions are never touched by install or uninstall.
Target amd64-unicode
!include "MUI2.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "1.10.0"
!endif
!ifndef SRC
  !define SRC "stage"
!endif

Name "Squelchmoth"
!ifndef OUTFILE
  !define OUTFILE "Squelchmoth-Setup-${VERSION}.exe"
!endif
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\Squelchmoth"
InstallDirRegKey HKLM "Software\Squelchmoth" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
BrandingText "Squelchmoth ${VERSION} - created by Jamar Stewart"
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "Squelchmoth"
VIAddVersionKey "CompanyName" "Jamar Stewart"
VIAddVersionKey "LegalCopyright" "(c) 2026 Jamar Stewart"
VIAddVersionKey "FileDescription" "Squelchmoth installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

!define MUI_ICON "${SRC}\squelchmoth.ico"
!define MUI_UNICON "${SRC}\squelchmoth.ico"
!define MUI_WELCOMEPAGE_TITLE "Install Squelchmoth ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "Squelchmoth is a complete studio: record, produce, mix and master, with Claude at the console.$\r$\n$\r$\nWithout a key it runs as the free Demo. Already bought? Open Help > Licences and Upgrades after installing and paste your key.$\r$\n$\r$\nClick Next to continue."
!define MUI_FINISHPAGE_RUN "$INSTDIR\Squelchmoth.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Open Squelchmoth now"
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\START HERE.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Read START HERE"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${SRC}\license.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "Squelchmoth needs 64-bit Windows 10 or 11." /SD IDOK
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Squelchmoth" SecMain
  SectionIn RO
  SetShellVarContext all
  ; quit a running copy so its files can be replaced
  nsExec::Exec 'taskkill /IM Squelchmoth.exe /F'
  SetOutPath "$INSTDIR"
  File "${SRC}\Squelchmoth.exe"
  File "${SRC}\cloudflared.exe"
  File "${SRC}\squelchmoth.ico"
  File "${SRC}\license.txt"
  File "${SRC}\PRIVACY.txt"
  File "${SRC}\THIRD-PARTY-NOTICES.txt"
  File "${SRC}\START HERE.txt"
  File "${SRC}\SETUP.md"
  File "${SRC}\PLUGINS.md"
  WriteUninstaller "$INSTDIR\Uninstall Squelchmoth.exe"

  CreateDirectory "$SMPROGRAMS\Squelchmoth"
  CreateShortcut "$SMPROGRAMS\Squelchmoth\Squelchmoth.lnk" "$INSTDIR\Squelchmoth.exe" "" "$INSTDIR\squelchmoth.ico"
  CreateShortcut "$SMPROGRAMS\Squelchmoth\Uninstall Squelchmoth.lnk" "$INSTDIR\Uninstall Squelchmoth.exe"
  CreateShortcut "$DESKTOP\Squelchmoth.lnk" "$INSTDIR\Squelchmoth.exe" "" "$INSTDIR\squelchmoth.ico"

  WriteRegStr HKLM "Software\Squelchmoth" "InstallDir" "$INSTDIR"
  !define UNINST "Software\Microsoft\Windows\CurrentVersion\Uninstall\Squelchmoth"
  WriteRegStr HKLM "${UNINST}" "DisplayName" "Squelchmoth"
  WriteRegStr HKLM "${UNINST}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINST}" "Publisher" "Jamar Stewart"
  WriteRegStr HKLM "${UNINST}" "DisplayIcon" "$INSTDIR\squelchmoth.ico"
  WriteRegStr HKLM "${UNINST}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINST}" "URLInfoAbout" "https://squelchmoth.com"
  WriteRegStr HKLM "${UNINST}" "UninstallString" '"$INSTDIR\Uninstall Squelchmoth.exe"'
  WriteRegDWORD HKLM "${UNINST}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINST}" "NoRepair" 1
SectionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  nsExec::Exec 'taskkill /IM Squelchmoth.exe /F'
  Delete "$INSTDIR\Squelchmoth.exe"
  Delete "$INSTDIR\cloudflared.exe"
  Delete "$INSTDIR\squelchmoth.ico"
  Delete "$INSTDIR\license.txt"
  Delete "$INSTDIR\PRIVACY.txt"
  Delete "$INSTDIR\THIRD-PARTY-NOTICES.txt"
  Delete "$INSTDIR\START HERE.txt"
  Delete "$INSTDIR\SETUP.md"
  Delete "$INSTDIR\PLUGINS.md"
  Delete "$INSTDIR\Uninstall Squelchmoth.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\Squelchmoth\Squelchmoth.lnk"
  Delete "$SMPROGRAMS\Squelchmoth\Uninstall Squelchmoth.lnk"
  RMDir "$SMPROGRAMS\Squelchmoth"
  Delete "$DESKTOP\Squelchmoth.lnk"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Squelchmoth"
  DeleteRegKey HKLM "Software\Squelchmoth"
  MessageBox MB_ICONINFORMATION "Squelchmoth is removed. Your sessions in Documents\Squelchmoth Sessions are still there." /SD IDOK
SectionEnd
