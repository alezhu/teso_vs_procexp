#include "service.h"

#include <format>

#include "consts.h"
#include <iostream>

#include "global.h"
#include "watcher.h"

#pragma comment(lib, "advapi32.lib")


SERVICE_STATUS_HANDLE ghStatus;
SERVICE_STATUS gsStatus;
HANDLE ghStopEvent = nullptr;

VOID ReportSvcStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint);

// Функция обработки управляющих сообщений сервиса
void WINAPI ServiceCtrlHandler(const DWORD dwOpCode) {
    if (SERVICE_CONTROL_STOP == dwOpCode) {
        const auto logger = getLogger();
        // Signal the service to stop.
        logger->info(L"ServiceCtrlHandler. ReportSvcStatus(SERVICE_STOP_PENDING)");
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);
        SetEvent(ghStopEvent);
        logger->info(L"ServiceCtrlHandler. Service Control stopped");
        ReportSvcStatus(gsStatus.dwCurrentState, NO_ERROR, 0);
    }
}

// Основная функция сервиса
void ServiceMain(
    DWORD dwNumServicesArgs,
    LPWSTR *lpServiceArgVectors
) {
    const auto logger = getLogger();
    // Регистрация обработчика событий
    logger->info(L"ServiceMain. RegisterServiceCtrlHandler");
    ghStatus = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (ghStatus == nullptr) {
        SvcReportLastError(const_cast<LPTSTR>(L"RegisterServiceCtrlHandler"));
        return;
    }
    // Установка начального состояния сервиса
    gsStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gsStatus.dwServiceSpecificExitCode = 0;
    gsStatus.dwCheckPoint = 0;

    // Report initial status to the SCM
    logger->info(L"ServiceMain. ReportSvcStatus(SERVICE_START_PENDING)");
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Create an event. The control handler function, SvcCtrlHandler,
    // signals this event when it receives the stop control code.
    logger->info(L"ServiceMain. CreateEvent");
    ghStopEvent = CreateEvent(
        nullptr, // default security attributes
        true, // manual reset event
        false, // not signaled
        nullptr); // no name

    if (ghStopEvent == nullptr) {
        logger->error(L"ServiceMain. CreateEvent");
        SvcReportLastError(const_cast<LPTSTR>(L"CreateEvent"));
        logger->info(L"ServiceMain. ReportSvcStatus(SERVICE_STOPPED)");
        ReportSvcStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    // Report running status when initialization is complete.
    logger->info(L"ServiceMain. ReportSvcStatus(SERVICE_RUNNING)");
    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    //Start TESO waiting thread.
    const Watcher watcher;
    logger->info(L"ServiceMain. Start watcher");
    if (watcher.start()) {
        logger->info(L"ServiceMain. Waiting stop event");
        // Perform work until service stops.
        WaitForSingleObject(ghStopEvent, INFINITE);
        logger->info(L"ServiceMain. Stop watcher");
        watcher.stop();
    }
    logger->info(L"ServiceMain. ReportSvcStatus(SERVICE_STOPPED)");
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void SvcReportLastError(LPCWSTR const szFunction) {
    const auto msg = std::format(L"{} failed with {}", szFunction, GetLastError());
    SvcReportEvent(msg.c_str());
}

void SvcReportEvent(LPCWSTR const szEvent) {
    const auto hEventSource = RegisterEventSourceW(nullptr, SERVICE_NAME);
    if (nullptr != hEventSource) {
        LPCWSTR lpszStrings[2];
        lpszStrings[0] = SERVICE_NAME;
        lpszStrings[1] = szEvent;

        ReportEventW(
            hEventSource, // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0, // event category
            0, // event identifier
            nullptr, // no security identifier
            2, // size of lpszStrings array
            0, // no binary data
            lpszStrings, // array of strings
            nullptr
        ); // no binary data

        DeregisterEventSource(hEventSource);
    }
    const auto logger = getLogger();
    logger->error(szEvent);
}

VOID ReportSvcStatus(const DWORD dwCurrentState,
                     const DWORD dwWin32ExitCode,
                     const DWORD dwWaitHint) {
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gsStatus.dwCurrentState = dwCurrentState;
    gsStatus.dwWin32ExitCode = dwWin32ExitCode;
    gsStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gsStatus.dwControlsAccepted = 0;
    else gsStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gsStatus.dwCheckPoint = 0;
    else gsStatus.dwCheckPoint = dwCheckPoint++;

    auto res = SetServiceStatus(ghStatus, &gsStatus);
    const auto logger = getLogger();
    const auto str = std::format(L"ReportSvcStatus. SetServiceStatus({}) -> {} ", dwCurrentState, res);
    logger->info(str);
}
