;--------------------------------
;Include Modern UI
  !include "TextFunc.nsh" ;Needed for the $GetSize function. I know, doesn't sound logical, it isn't.
  !include "MUI2.nsh"

;--------------------------------
;Variables

  !define PRODUCT_NAME "Raycoin"
  !define PRODUCT_WEB_SITE "http://ray-coin.com"
  !define PRODUCT_PUBLISHER "NewGamePlus Inc."
  !define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

;--------------------------------
;General

  ;Name and file
  Name "${PRODUCT_NAME}"
  OutFile "../../../dist/package/win/Raycoin-${PRODUCT_VERSION}-setup.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"

  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\${PRODUCT_NAME}" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

  ;Specifies whether or not the installer will perform a CRC on itself before allowing an install
  CRCCheck on
  
  ;Sets whether or not the details of the install are shown. Can be 'hide' (the default) to hide the details by default, allowing the user to view them, or 'show' to show them by default, or 'nevershow', to prevent the user from ever seeing them.
  ShowInstDetails show
  
  ;Sets whether or not the details of the uninstall  are shown. Can be 'hide' (the default) to hide the details by default, allowing the user to view them, or 'show' to show them by default, or 'nevershow', to prevent the user from ever seeing them.
  ShowUninstDetails show
  
  ;Sets the colors to use for the install info screen (the default is 00FF00 000000. Use the form RRGGBB (in hexadecimal, as in HTML, only minus the leading '#', since # can be used for comments). Note that if "/windows" is specified as the only parameter, the default windows colors will be used.
  InstallColors /windows
  
  ;This command sets the compression algorithm used to compress files/data in the installer. (http://nsis.sourceforge.net/Reference/SetCompressor)
  SetCompressor /SOLID lzma
  
  ;Sets the dictionary size in megabytes (MB) used by the LZMA compressor (default is 8 MB).
  SetCompressorDictSize 64
  
  ;Sets the text that is shown (by default it is 'Nullsoft Install System vX.XX') in the bottom of the install window. Setting this to an empty string ("") uses the default; to set the string to blank, use " " (a space).
  BrandingText "${PRODUCT_NAME} Installer v${PRODUCT_VERSION}" 
  
  ;Sets what the titlebars of the installer will display. By default, it is 'Name Setup', where Name is specified with the Name command. You can, however, override it with 'MyApp Installer' or whatever. If you specify an empty string (""), the default will be used (you can however specify " " to achieve a blank string)
  Caption "${PRODUCT_NAME}"

  ;Adds the Product Version on top of the Version Tab in the Properties of the file.
  VIProductVersion 1.0.0.0
  
  ;VIAddVersionKey - Adds a field in the Version Tab of the File Properties. This can either be a field provided by the system or a user defined field.
  VIAddVersionKey ProductName "${PRODUCT_NAME} Installer"
  VIAddVersionKey Comments "The installer for ${PRODUCT_NAME}"
  VIAddVersionKey CompanyName "${PRODUCT_NAME}"
  VIAddVersionKey LegalCopyright "2019 ${PRODUCT_PUBLISHER}"
  VIAddVersionKey FileDescription "${PRODUCT_NAME} Installer"
  VIAddVersionKey FileVersion ${PRODUCT_VERSION}
  VIAddVersionKey ProductVersion ${PRODUCT_VERSION}
  VIAddVersionKey InternalName "${PRODUCT_NAME} Installer"
  VIAddVersionKey LegalTrademarks "${PRODUCT_NAME} is a trademark of ${PRODUCT_PUBLISHER}" 
  VIAddVersionKey OriginalFilename "${PRODUCT_NAME}d.exe"

;--------------------------------
;Interface Settings
 
  !define MUI_ABORTWARNING
  !define MUI_ABORTWARNING_TEXT "Are you sure you wish t o abort the installation of ${PRODUCT_NAME}?"
  
  !define MUI_ICON "..\..\..\viewer\src\raycoin.ico"
  !define MUI_COMPONENTSPAGE_NODESC

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

  !define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
  !define MUI_FINISHPAGE_SHOWREADME_TEXT "show release notes"
  !define MUI_FINISHPAGE_SHOWREADME $INSTDIR\release-notes.txt
  !define MUI_FINISHPAGE_RUN
  !define MUI_FINISHPAGE_RUN_TEXT "mine Raycoin now"
  !define MUI_FINISHPAGE_RUN_FUNCTION "Run"
  !insertmacro MUI_PAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Raycoin" raycoin
  SetOutPath $INSTDIR

  DetailPrint "Uninstalling old version files..."
  RMDir /r "$INSTDIR"
  DetailPrint "Removing old version shortcuts..."
  Delete "$DESKTOP\${PRODUCT_NAME} - mine.lnk"
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
  
  DetailPrint "Installing new files..."
  File /r "..\..\..\build\package\win\*.*"
  File "..\..\..\viewer\src\raycoin.ico"

  ;Store installation folder
  WriteRegStr HKCU "Software\${PRODUCT_NAME}" "" $INSTDIR

  DetailPrint "Creating uninstaller..."
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  IfFileExists "$APPDATA\${PRODUCT_NAME}\settings.ps1" +4 0
  DetailPrint "Creating user settings..."
  CreateDirectory "$APPDATA\${PRODUCT_NAME}"
  CopyFiles "$INSTDIR\scripts\settings.ps1" "$APPDATA\${PRODUCT_NAME}\settings.ps1"

  DetailPrint "Creating shortcuts for scripts..."
  CreateShortCut "$INSTDIR\scripts\${PRODUCT_NAME}.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\raycoind.ps1'$\"" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$INSTDIR\scripts\${PRODUCT_NAME} Console.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "" "$INSTDIR\raycoin.ico" 0

  DetailPrint "Creating start-menu items..."
  CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\raycoind.ps1'$\"" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} Console.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} Viewer.lnk" "$INSTDIR\RaycoinViewer.exe"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} - mine.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\generate.ps1'$\"" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} - mine in background.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\generate.ps1' background$\"" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} - mine at full speed.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\generate.ps1' fullspeed$\"" "$INSTDIR\raycoin.ico" 0
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} - stop mining.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\stop-generate.ps1'$\""
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\settings.lnk" "$APPDATA\${PRODUCT_NAME}\settings.ps1"
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\z) export rewards to Electrum.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\export-electrum.ps1'$\""

  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Windows startup - run ${PRODUCT_NAME}.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1'$\""
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Windows startup - mine.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' generate$\""
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Windows startup - mine in background.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' generate background$\""
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Windows startup - mine at full speed.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' generate fullspeed$\""
  CreateShortCut "$SMPROGRAMS\${PRODUCT_NAME}\Windows startup - remove.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' remove$\""

  ;Adds an uninstaller possibility to Windows Uninstall or change a program section
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\raycoin.ico"

  ;Fixes Windows broken size estimates
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"

  DetailPrint "Removing old version from Windows startup..."
  ExecWait "$\"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe$\" -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' remove hidden$\""
SectionEnd

Section "desktop shortcut" desktop_shortcut
  DetailPrint "Creating desktop shortcut..."
  CreateShortCut "$DESKTOP\${PRODUCT_NAME} - mine.lnk" "$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\generate.ps1'$\"" "$INSTDIR\raycoin.ico" 0
SectionEnd

Section "mine Raycoin at Windows startup" startup_mine
  DetailPrint "Adding Raycoin mining to Windows startup..."
  ExecWait "$\"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe$\" -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' generate hidden$\""
SectionEnd

;Check if we have Administrator rights
Function .onInit
  UserInfo::GetAccountType
  pop $0
  ${If} $0 != "admin" ;Require admin rights on NT4+
    MessageBox mb_iconstop "Administrator rights required!"
    SetErrorLevel 740 ;ERROR_ELEVATION_REQUIRED
    Quit
  ${EndIf}

  IntOp $0 ${SF_SELECTED} | ${SF_RO}
  SectionSetFlags ${raycoin} $0
FunctionEnd

Function Run
  ExecShell "" "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} - mine.lnk"
FunctionEnd

;--------------------------------
;Descriptions

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  DetailPrint "Removing from Windows startup..."
  ExecWait "$\"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe$\" -WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -Command $\"& '$INSTDIR\scripts\startup.ps1' remove hidden$\""

  DetailPrint "Uninstalling files..."
  RMDir /r "$INSTDIR"
  DetailPrint "Removing shortcuts..."
  Delete "$DESKTOP\${PRODUCT_NAME} - mine.lnk"
  RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"

  DeleteRegKey HKCU "Software\${PRODUCT_NAME}"
  DeleteRegKey HKCU "${PRODUCT_UNINST_KEY}"
SectionEnd
