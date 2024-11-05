!include "LogicLib.nsh"
!include "x64.nsh"

!define VERSION "1.0.0"
!define SERVICE_NAME "TESOWaitService"
!define DISPLAY_NAME "TESOWaitService"
Name "${DISPLAY_NAME} Installer"
OutFile "TESO-Wait-Service-Installer_${VERSION}.exe"
RequestExecutionLevel admin ; Запрашиваем права администратора
Icon "..\service\assets\product.ico"

InstallDir "$PROGRAMFILES\${DISPLAY_NAME}"

Section
    SetShellVarContext all

    Call DeleteService

    SetOutPath $INSTDIR
    File /r "..\cmake-build-release\service\*.exe" ; Укажите путь к вашим файлам сервиса

    DetailPrint "Create service..."
    nsExec::ExecToStack 'sc create ${SERVICE_NAME} binPath= "$INSTDIR\teso_wait_service.exe" DisplayName= "${DISPLAY_NAME}" start= "delayed-auto"'
    Pop $0
    ${If} $0 <> 0
        DetailPrint "Error creating service: $0"
        Abort
    ${EndIf}


    DetailPrint "Start sevice..."
    nsExec::ExecToStack 'sc start ${SERVICE_NAME}'
    Pop $0
    ${If} $0 <> 0
        DetailPrint "Error starting service: $0"
        Abort
    ${EndIf}

    WriteUninstaller $INSTDIR\uninstaller.exe

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}" "DisplayName" "${DISPLAY_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}" "UninstallString" '"$INSTDIR\uninstaller.exe"'
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}" "DisplayIcon" "$INSTDIR\teso_wait_service.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}" "Publisher" "alezhu"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}" "DisplayVersion" "${VERSION}"


    DetailPrint "Service installed and started"
SectionEnd

Section "Uninstall"
  Call un.DeleteService
  Delete $INSTDIR\uninstaller.exe ; delete self (see explanation below why this works)
  Delete $INSTDIR\teso_wait_service.exe
  RMDir $INSTDIR
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${SERVICE_NAME}"
SectionEnd

Function DeleteService
    DetailPrint "Stop service..."
    nsExec::ExecToStack 'sc stop ${SERVICE_NAME}'
    Pop $0
    ${If} $0 == 0
        DetailPrint "Delete service..."
        nsExec::ExecToStack 'sc delete ${SERVICE_NAME}'
        Pop $0
    ${EndIf}
FunctionEnd

Function un.DeleteService
    DetailPrint "Stop service..."
    nsExec::ExecToStack 'sc stop ${SERVICE_NAME}'
    Pop $0
    ${If} $0 == 0
        DetailPrint "Delete service..."
        nsExec::ExecToStack 'sc delete ${SERVICE_NAME}'
        Pop $0
    ${EndIf}
FunctionEnd
