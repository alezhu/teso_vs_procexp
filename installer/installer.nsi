!include "LogicLib.nsh"
!include "x64.nsh"

Var SERVICE_NAME
Name "TESO Wait Service Installer"
OutFile "TESO-Wait-Service-Installer.exe"
RequestExecutionLevel admin ; Запрашиваем права администратора

InstallDir "$PROGRAMFILES\TESO Wait Service"

Function .onInit
  StrCpy $SERVICE_NAME "TESOWaitService"
FunctionEnd

Function un.onInit
  StrCpy $SERVICE_NAME "TESOWaitService"
FunctionEnd


Section
    SetShellVarContext all

    Call DeleteService

    SetOutPath $INSTDIR
    File /r "..\cmake-build-release\service\*.exe" ; Укажите путь к вашим файлам сервиса

    DetailPrint "Create service..."
    nsExec::ExecToStack 'sc create $SERVICE_NAME binPath= "$INSTDIR\teso_wait_service.exe" DisplayName= "TESO Wait Service" start= "delayed-auto"'
    Pop $0
    ${If} $0 <> 0
        DetailPrint "Error creating service: $0"
        Abort
    ${EndIf}


    DetailPrint "Start sevice..."
    nsExec::ExecToStack 'sc start $SERVICE_NAME'
    Pop $0
    ${If} $0 <> 0
        DetailPrint "Error starting service: $0"
        Abort
    ${EndIf}

    WriteUninstaller $INSTDIR\uninstaller.exe

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$SERVICE_NAME" "DisplayName" "TESO Wait Service"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\$SERVICE_NAME" "UninstallString" '"$INSTDIR\uninstaller.exe"'


    DetailPrint "Service installed and started"
SectionEnd

Section "Uninstall"
  Call un.DeleteService
  Delete $INSTDIR\Uninst.exe ; delete self (see explanation below why this works)
  Delete $INSTDIR\teso_wait_service.exe
  RMDir $INSTDIR
SectionEnd

Function DeleteService
    DetailPrint "Stop service..."
    nsExec::ExecToStack 'sc stop $SERVICE_NAME'
    Pop $0
    ${If} $0 == 0
        DetailPrint "Delete service..."
        nsExec::ExecToStack 'sc delete $SERVICE_NAME'
        Pop $0
    ${EndIf}
FunctionEnd

Function un.DeleteService
    DetailPrint "Stop service..."
    nsExec::ExecToStack 'sc stop $SERVICE_NAME'
    Pop $0
    ${If} $0 == 0
        DetailPrint "Delete service..."
        nsExec::ExecToStack 'sc delete $SERVICE_NAME'
        Pop $0
    ${EndIf}
FunctionEnd
