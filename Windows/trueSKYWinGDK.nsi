!include "MUI.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; The name of the installer
Name "Simul trueSKY for Win GDK"
!ifndef SIMUL_VERSION
!define SIMUL_VERSION '4.3.1.0026'
!endif
!ifndef GDK_VERSION
!define GDK_VERSION '200806'
!endif
!ifndef SIMUL
!define SIMUL C:\Simul\4.3\Simul
!endif
!ifndef SIMUL_BUILD
!define SIMUL_BUILD ${SIMUL}\build_wingdk_${GDK_VERSION}
!endif
!ifndef OUTPUT_FILE
!define OUTPUT_FILE 'trueSKYWin_GDK${GDK_VERSION}_${SIMUL_VERSION}.exe'
!endif

; The file to write
; ${StrReplace} "${OUTPUT_FILE}" "\" "//"
OutFile "${OUTPUT_FILE}"
Icon "${SIMUL}\Products\Simul Icon.ico"

; The default installation directory
InstallDir "C:\Simul"
RequestExecutionLevel admin

!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_BITMAP "${SIMUL}\Products\Simul Logo 150x57.bmp"

!define MUI_ABORTWARNING

!define WELCOME_TITLE 'Simul trueSKY for Win GDK ${SIMUL_VERSION}'
!define MUI_ICON "${SIMUL}\Products\Simul Icon.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${SIMUL}\Products\Simul Logo 164x314.bmp" 
!define MUI_WELCOMEPAGE_TITLE '${WELCOME_TITLE}'
!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH
!define MUI_WELCOMEPAGE_TEXT "This wizard will install required files for Simul trueSKY for Win GDK,.\n\nlinked against the ${GDK_VERSION} GDK."
!define MUI_DIRECTORYPAGE_TEXT_TOP "Please identify the Simul directory."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE "English"


Section "-Documentation"
	SetOutPath "$INSTDIR\Simul"
	File "/oname=ReadMe.txt" "${SIMUL}\ReadMeBinaryLicense.md"
	File "${SIMUL}\Products\TrueSky\ChangeList.txt"
	IfFileExists "$INSTDIR\Simul\LicenseKey.h" +2
		File "/oname=LicenseKey.h" "${SIMUL}\DeployLicenseKey.h"
SectionEnd

Section "-Headers"
	SetOutPath $INSTDIR\Simul
	
	SetOutPath $INSTDIR\Simul\Products\TrueSky
	File /nonfatal /r "${SIMUL}\Products\TrueSky\Version.h"
	
	SetOutPath $INSTDIR\Simul\Base
	File /r "${SIMUL}\Base\*.h"
	CreateDirectory "$INSTDIR\Simul\Clouds"
	SetOutPath "$INSTDIR\Simul\Clouds"
	File /r "${SIMUL}\Clouds\*.h"
	SetOutPath $INSTDIR\Simul\Platform\Math
	File /r "${SIMUL}\Platform\Math\*.h"
	File /r "${SIMUL}\Platform\Math\*.inl"
	SetOutPath $INSTDIR\Simul\Geometry
	File /nonfatal /r "${SIMUL}\Geometry\*.h"
	SetOutPath $INSTDIR\Simul\Sky
	File /r "${SIMUL}\Sky\*.h"
	SetOutPath $INSTDIR\Simul\Terrain
	File /r "${SIMUL}\Terrain\*.h"

	SetOutPath $INSTDIR\Simul\Tools\Setup
	File /nonfatal "${SIMUL}\Tools\Setup\*.rules"
	SetOutPath $INSTDIR\Simul\Products
	File /nonfatal "${SIMUL}\Products\Simul Icon.ico"
SectionEnd

Section "-CrossPlatform"
	SetOutPath $INSTDIR\Simul\Media
	SetOutPath $INSTDIR\Simul\Platform\CrossPlatform
	File /nonfatal /r "${SIMUL}\Platform\CrossPlatform\*.h"
	File /nonfatal /r "${SIMUL}\Platform\CrossPlatform\*.sfx"
	File /nonfatal /r "${SIMUL}\Platform\CrossPlatform\*.sl"
	SetOutPath $INSTDIR\Simul\Platform\DirectX12
	File /r "${SIMUL}\Platform\DirectX12\*.h"
	File /r "${SIMUL}\Platform\DirectX12\*.sl"
	SetOutPath $INSTDIR\Simul\Platform\DirectX12\shaderbin
	File /r "${SIMUL_BUILD}\Platform\DirectX12\shaderbin\*.*"
	SetOutPath $INSTDIR\Simul\Platform\DirectX11
	File /r "${SIMUL}\Platform\DirectX11\*.h"
	File /r "${SIMUL}\Platform\DirectX11\*.sl"
	SetOutPath $INSTDIR\Simul\Platform\DirectX11\shaderbin
	File /r "${SIMUL_BUILD}\Platform\DirectX11\shaderbin\*.*"
SectionEnd
 
Section "-Built"	
	SetOutPath $INSTDIR\Simul\build_WinGDK\bin
	File /r "${BUILD_DIR}\bin\*.*"
	SetOutPath $INSTDIR\Simul\build_WinGDK\lib
	File /r "${BUILD_DIR}\lib\*.*"
SectionEnd


Function .onInstSuccess
	IfSilent +2 0
		ExecShell "start" "" "http://docs.simul.co"
FunctionEnd
