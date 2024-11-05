#include <windows.h>


#ifndef SERVICE_H
#define SERVICE_H

void ServiceMain(DWORD dwNumServicesArgs, LPWSTR *lpServiceArgVectors);

void SvcReportEvent(LPCTSTR szEvent);

void SvcReportLastError(LPCTSTR szFunction);
#endif //SERVICE_H
