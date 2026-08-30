!include MUI2.nsh
!include LogicLib.nsh
!include x64.nsh
!include WinVer.nsh

# The install path is hardcoded.
# We need both a x64 and x86 OpenWith.exe and putting both
# in Program Files won't work. Having two directory choices
# would also be a mess.

Unicode true
Name "AuthXP"
Outfile "build\AuthXP-setup-x64.exe"
RequestExecutionLevel admin
ManifestSupportedOS all

!define MUI_ICON "installer.ico"
!define MUI_UNICON "installer.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_UNWELCOMEFINISHPAGE_BITMAP "welcome.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "header.bmp"
!define MUI_UNHEADERIMAGE_BITMAP "header.bmp"
!define MUI_ABORTWARNING
!define MUI_UNABORTWARNING
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!macro LANG_LOAD LANGLOAD
    !insertmacro MUI_LANGUAGE "${LANGLOAD}"
    !include "l10n\${LANGLOAD}.nsh"
    !undef LANG
!macroend
 
!macro LANG_STRING NAME VALUE
    LangString "${NAME}" "${LANG_${LANG}}" "${VALUE}"
!macroend

!insertmacro LANG_LOAD "English"
!insertmacro LANG_LOAD "French"
!insertmacro LANG_LOAD "Italian"
!insertmacro LANG_LOAD "Korean"
!insertmacro LANG_LOAD "Japanese"

Section "AuthXP" AuthXP
	SectionIn RO
    # Make sure install directories are clean
    RMDir /r "$INSTDIR\"

    # Install x86-64 files
    SetOutPath "$INSTDIR\"
    WriteUninstaller "$INSTDIR\uninstall.exe"
	
	${If} ${AtLeastBuild} 17763
	${AndIf} ${AtMostBuild} 18361
		File "..\x64\Release.RS5\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 18362
	${AndIf} ${AtMostBuild} 19040
		File "..\x64\Release.19H1\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 19041
	${AndIf} ${AtMostBuild} 19045
		File "..\x64\Release.VB\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 20348
	${AndIf} ${AtMostBuild} 21999
		File "..\x64\Release.FE\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 22000
	${AndIf} ${AtMostBuild} 22620
		File "..\x64\Release.CO\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 22621
	${AndIf} ${AtMostBuild} 22630
		File "..\x64\Release.NI\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 22631
	${AndIf} ${AtMostBuild} 26099
		File "..\x64\Release.ZN\AuthXP.dll"
	${ElseIf} ${AtLeastBuild} 26100
	${AndIf} ${AtMostBuild} 26201
		File "..\x64\Release.GE\AuthXP.dll"
	${EndIf}
	
    # Create Uninstall entry
    SetRegView 64
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "DisplayName" "AuthXP"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "Publisher" "wiktorwiktor12"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "DisplayVersion" "0.0.1"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "NoModify" 1
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP" \
                 "NoRepair" 1

    # Make LogonController use our server
    ReadEnvStr $0 "USERNAME"
    AccessControl::SetRegKeyOwner HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" $0
    AccessControl::GrantOnRegKey HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" $0 FullAccess
    WriteRegExpandStr HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" \
        "DllPath" "$INSTDIR\AuthXP.dll"
	
	AccessControl::SetRegKeyOwner HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" $0
    AccessControl::GrantOnRegKey HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" $0 FullAccess
    WriteRegExpandStr HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" \
        "DllPath" "$INSTDIR\AuthXP.dll" 
SectionEnd

Section /o "ClassicShutdown" ClassicShutdown
    SetOutPath "$INSTDIR\en-us"
    File "en-US\ClassicShutdown.dll.mui"
	SetOutPath "$INSTDIR\ja-JP"
    File "ja-JP\ClassicShutdown.dll.mui"

    SetOutPath "$INSTDIR\"
    File "classicshutdown.dll"
SectionEnd

Section "Uninstall"
    # Delete files
    RMDir /r "$INSTDIR"

    # Revert to default LogonController server.
    SetRegView 64
    ReadEnvStr $0 "USERNAME"
    AccessControl::SetRegKeyOwner HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" $0
    AccessControl::GrantOnRegKey HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" $0 FullAccess
    WriteRegExpandStr HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LogonUX" \
        "DllPath" "%SystemRoot%\system32\Windows.UI.Logon.dll"
		
	AccessControl::SetRegKeyOwner HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" $0
    AccessControl::GrantOnRegKey HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" $0 FullAccess
    WriteRegExpandStr HKLM "SOFTWARE\Microsoft\WindowsRuntime\ActivatableClassId\Windows.Internal.UI.Logon.Controller.LockScreenHost" \
        "DllPath" "%SystemRoot%\system32\logoncontroller.dll"

    # Delete uninstall entry
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AuthXP"
SectionEnd

Function .onInit
    # NSIS produces an x86-32 installer. Deny installation if
    # we're not on a x86-64 system running WOW64.
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "$(STRING_NOT_X64)"
        Quit
    ${EndIf}
    
    # Need at least Windows 10 1809 and anything below Germanium (build 26100).
    ${IfNot} ${AtLeastBuild} 17763
	${OrIfNot} ${AtMostBuild} 26201
        MessageBox MB_OK|MB_ICONSTOP "$(STRING_NOT_SUP)"
        Quit
    ${EndIf}
	
	SectionSetSize ${AuthXP} 4800
	SectionSetSize ${ClassicShutdown} 100
	
	StrCpy $INSTDIR "$PROGRAMFILES64\AuthXP"
FunctionEnd
