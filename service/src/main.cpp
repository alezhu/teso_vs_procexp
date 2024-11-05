
#include "global.h"
#include "service.h"
#include "consts.h"

int main() {

    constexpr SERVICE_TABLE_ENTRYW serviceTable[] =
    {
        {const_cast<LPWSTR>(SERVICE_NAME), static_cast<LPSERVICE_MAIN_FUNCTIONW>(ServiceMain)},
        {nullptr, nullptr}
    };
    // Установка сервиса
    const auto logger = getLogger();
    logger->init();
    logger->info(L"main(). StartServiceCtrlDispatcher");
    if(0 == StartServiceCtrlDispatcherW(serviceTable)) {
        SvcReportLastError(L"main(). StartServiceCtrlDispatcher");
        if(ERROR_FAILED_SERVICE_CONTROLLER_CONNECT != GetLastError()) {
            return 4;
        }
    }
    return 0;
}
