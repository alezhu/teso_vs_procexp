#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1 // Exclude rarely-used APIs from <windows.h>
#endif
#include <windows.h>
#define INITGUID // Ensure that EventTraceGuid is defined.
#include <evntrace.h>
#undef INITGUID

#include "watcher.h"

#include <algorithm>
#include <chrono>
#include <string_view>
#include <tdh.h>
#include <evntcons.h>
#include <evntprov.h>
#include "global.h"
#include "utils.h"
#include <tlhelp32.h>

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")

// constexpr std::wstring_view NTKernelLogger{KERNEL_LOGGER_NAME};
 constexpr auto EXPECTED_PROCESS_NAME{"ESO64.EXE"};
// constexpr auto EXPECTED_PROCESS_NAME{"NOTEPAD++.EXE"};
constexpr std::wstring_view CLOSABLE_PROCESS_NAME{L"PROCEXP64.EXE"};
volatile bool bStop = false;

void CloseProcessByName(std::wstring_view processName) {
    const auto logger = getLogger();
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                std::wstring exeFile(pe.szExeFile);
                exeFile.assign(pe.szExeFile);
                std::ranges::transform(exeFile, exeFile.begin(), ::toupper);
                if (exeFile == processName) {
                    logger->info(L"CloseProcessByName. Found process: " + exeFile);
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (hProcess != nullptr) {
                        logger->info(L"CloseProcessByName. Terminate process: " + std::wstring(exeFile));
                        if (!TerminateProcess(hProcess, 0)) {
                            logger->error(L"CloseProcessByName. Can't teminate process");
                        }
                        CloseHandle(hProcess);
                        //break;
                    } else {
                        logger->error(L"CloseProcessByName. Can't open process");
                    }
                }
            } while (Process32Next(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    } else {
        logger->error(L"CloseProcessByName. Can't open process snapshot");
    }
}

// Callback функция для обработки событий
void WINAPI ProcessEvent(PEVENT_RECORD psEventRecord) {
    if (bStop) return;

    if (psEventRecord->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_INFO
        && psEventRecord->EventHeader.ProviderId == EventTraceGuid) {
        return;
    }
    // Проверка на событие создания процесса (Opcode 1)
    if (psEventRecord->EventHeader.EventDescriptor.Opcode == 1) {
        const auto logger = getLogger();

        DWORD dwPropertySize = 0;
        PROPERTY_DATA_DESCRIPTOR sPropertyDataDescriptor{0};
        sPropertyDataDescriptor.PropertyName = reinterpret_cast<decltype(sPropertyDataDescriptor.PropertyName)>(
            L"ImageFileName");
        TdhGetPropertySize(psEventRecord, 0, nullptr, 1, &sPropertyDataDescriptor, &dwPropertySize);

        // const std::wstring processName(propertySize / sizeof(WCHAR), L'\0');;
        std::string processName(dwPropertySize, '\0');;
        if (TdhGetProperty(
                psEventRecord,
                0,
                nullptr,
                1,
                &sPropertyDataDescriptor,
                dwPropertySize,
                PBYTE(processName.c_str())
            ) == ERROR_SUCCESS) {
            std::ranges::transform(processName, processName.begin(), ::toupper);
            if (processName.find(EXPECTED_PROCESS_NAME) != std::string::npos) {
                logger->info(L"Process started: " + a2w(std::string(processName)));
                CreateThread(nullptr,0,[](PVOID pParam)->DWORD {
                    CloseProcessByName(CLOSABLE_PROCESS_NAME);
                    return 0;
                }, nullptr, 0, nullptr);
            }
        }
    }
}

class Watcher::WatcherPrivate {
public:
    bool start() {
        std::wstring_view loggerName{L"TESO Watcher Session"};
        m_hTrace = INVALID_PROCESSTRACE_HANDLE;
        const auto logger = getLogger();
        logger->info(L"Watcher.start() enter");
#pragma pack(1)
        struct TProperty {
            EVENT_TRACE_PROPERTIES base;
            TCHAR name[1024];
        };
#pragma pack()

        TProperty sProperty = {0};
        sProperty.base.Wnode.BufferSize = sizeof(sProperty);
        sProperty.base.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        sProperty.base.LoggerNameOffset = offsetof(TProperty, name);
        constexpr GUID ProcessTraceGuid{0x3d6fa8d1, 0xfe05, 0x11d0, {0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x29}};
        sProperty.base.Wnode.Guid = ProcessTraceGuid; //SystemTraceControlGuid; //ProviderGuid;
        sProperty.base.Wnode.ClientContext = 1; //QPC clock resolution
        sProperty.base.Wnode.Flags = WNODE_FLAG_TRACED_GUID;
        sProperty.base.BufferSize = 1024;
        sProperty.base.MinimumBuffers = 2;
        sProperty.base.MaximumBuffers = 64;
        sProperty.base.LogFileMode =
                EVENT_TRACE_SYSTEM_LOGGER_MODE | EVENT_TRACE_FILE_MODE_SEQUENTIAL | PROCESS_TRACE_MODE_REAL_TIME;
        sProperty.base.EnableFlags = EVENT_TRACE_FLAG_PROCESS; //PROCESS_EVENT_TYPE_INFO;
        lstrcpyn(sProperty.name, loggerName.data(), sizeof(sProperty.name));


        TProperty sTemp{sProperty};
        ControlTraceW(0, loggerName.data(), &sTemp.base, EVENT_TRACE_CONTROL_STOP);
        logger->info(L"Watcher.start(). StartTrace");
        auto rc = StartTraceW(&m_hSession, loggerName.data(), &sProperty.base);
        switch (rc) {
            case ERROR_SUCCESS:
            case ERROR_ALREADY_EXISTS:
                break;
            default:
                logger->error(std::format(L"Watcher.start(). Can't start trace. RC: {}", rc));
                return false;
        }

        EVENT_TRACE_LOGFILE sLogFile = {nullptr};
        sLogFile.LoggerName = const_cast<decltype(sLogFile.LoggerName)>(loggerName.data());
        sLogFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
        sLogFile.EventRecordCallback = reinterpret_cast<PEVENT_RECORD_CALLBACK>(ProcessEvent);
        sLogFile.IsKernelTrace = true;
        logger->info(L"Watcher.start(). OpenTrace");
        m_hTrace = OpenTraceW(&sLogFile);
        if (INVALID_PROCESSTRACE_HANDLE == m_hTrace) {
            logger->error(std::format(L"Watcher.start(). Can't open trace. RC: {}", rc));
            return false;
        }

        bStop = false;
        m_hThread = CreateThread(nullptr, 0, [](LPVOID pParam) -> DWORD {
            const auto logger = getLogger();
            auto *phTrace = static_cast<TRACEHANDLE *>(pParam);
            logger->info(L"ProcessTrace start");
            ProcessTrace(phTrace, 1, nullptr, nullptr);
            logger->info(L"ProcessTrace stop");
            return 0;
        }, &m_hTrace, 0, nullptr);

        if (m_hThread == nullptr) {
            logger->error(L"Watcher.start(). CreateThread failed");
            stop();
            return false;
        }
        logger->info(L"Watcher.start() exit");
        return true;
    }

    void stop() {
        const auto logger = getLogger();
        logger->info(L"Watcher.stop() enter");

        bStop = true;
        if (INVALID_PROCESSTRACE_HANDLE != m_hTrace) {
            logger->info(L"Watcher.stop(). CloseTrace");
            CloseTrace(m_hTrace);
            m_hTrace = INVALID_PROCESSTRACE_HANDLE;
        }

        if (m_hSession != 0) {
            logger->info(L"Watcher.stop(). ControlTraceW");
            ControlTraceW(m_hSession, nullptr, nullptr, EVENT_TRACE_CONTROL_STOP);
            m_hSession = 0;
        }

        if (m_hThread != nullptr) {
            logger->info(L"Watcher.stop(). Waiting thread stop");
            WaitForSingleObject(m_hThread, 10000);
            CloseHandle(m_hThread);
            m_hThread = nullptr;
        }


        logger->info(L"Watcher.stop() exit");
    }

    WatcherPrivate()
        : m_hThread(nullptr),
          m_hTrace{INVALID_PROCESSTRACE_HANDLE} {
    }

    ~WatcherPrivate() {
        stop();
    }

private:
    HANDLE m_hThread;
    TRACEHANDLE m_hTrace;
    TRACEHANDLE m_hSession{0};
};

Watcher::Watcher(): d{std::make_unique<WatcherPrivate>()} {
}

Watcher::~Watcher() = default;

bool Watcher::start() const {
    return d->start();
}

void Watcher::stop() const {
    d->stop();
}
