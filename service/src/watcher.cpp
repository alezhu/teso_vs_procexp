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
#include <atomic>
#include <mutex>
#include <thread>
#include <array>

#pragma comment(lib, "tdh.lib")
#pragma comment(lib, "advapi32.lib")

constexpr auto EXPECTED_PROCESS_NAME{"ESO64.EXE"};
constexpr std::wstring_view CLOSABLE_PROCESS_NAME{L"PROCEXP64.EXE"};

// Known process name variants for robust matching
constexpr std::array<std::wstring_view, 3> ESO_NAMES = {
    L"ESO64.EXE", L"ESO.EXE", L"ELDERSCROLLSOnline.EXE"
};
constexpr std::array<std::wstring_view, 2> PROCEXP_NAMES = {
    L"PROCEXP64.EXE", L"PROCEXP.EXE"
};

std::atomic<bool> bStop{false};
std::atomic<bool> g_terminationScheduled{false};
std::mutex g_terminationMutex;
HANDLE g_pollingThreadHandle = nullptr;
std::atomic<bool> g_pollingActive{false};

// Helper: case-insensitive match against known names
bool IsTargetProcess(std::wstring_view exeFileUpper) {
    for (auto name : ESO_NAMES) {
        if (exeFileUpper == name) return true;
    }
    return false;
}

bool IsProcexpProcess(std::wstring_view exeFileUpper) {
    for (auto name : PROCEXP_NAMES) {
        if (exeFileUpper == name) return true;
    }
    return false;
}

// Retry TerminateProcess with exponential backoff
bool TerminateProcessWithRetry(DWORD pid, int maxRetries = 5) {
    const auto logger = getLogger();
    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess) {
            if (TerminateProcess(hProcess, 0)) {
                logger->info(std::format(L"TerminateProcess succeeded for PID {} on attempt {}", pid, attempt + 1));
                CloseHandle(hProcess);
                return true;
            } else {
                DWORD err = GetLastError();
                logger->warning(std::format(L"TerminateProcess failed for PID {} (attempt {}/{}), error: {}", pid, attempt + 1, maxRetries, err));
            }
            CloseHandle(hProcess);
        } else {
            DWORD err = GetLastError();
            logger->warning(std::format(L"OpenProcess failed for PID {} (attempt {}/{}), error: {}", pid, attempt + 1, maxRetries, err));
        }
        // Exponential backoff: 200, 400, 800, 1600, 3200 ms
        DWORD sleepMs = 200 * (1 << attempt);
        Sleep(sleepMs);
    }
    logger->error(std::format(L"TerminateProcess failed for PID {} after {} attempts", pid, maxRetries));
    return false;
}

// Single worker: find and terminate Procexp with retry
DWORD WINAPI TerminationWorker(LPVOID) {
    const auto logger = getLogger();
    
    // Initial delay to let Procexp appear in process list
    Sleep(500);
    
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (bStop.load()) break;
        
        bool found = false;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{sizeof(PROCESSENTRY32W)};
            if (Process32FirstW(hSnapshot, &pe)) {
                do {
                    std::wstring exeFile(pe.szExeFile);
                    std::ranges::transform(exeFile, exeFile.begin(), ::toupper);
                    if (IsProcexpProcess(exeFile)) {
                        found = true;
                        logger->info(std::format(L"Found Procexp: {} (PID: {})", exeFile, pe.th32ProcessID));
                        TerminateProcessWithRetry(pe.th32ProcessID);
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
        if (found) break;
        
        // Backoff: 300, 600, 900, 1200, 1500 ms
        DWORD sleepMs = 300 * (attempt + 1);
        logger->debug(std::format(L"Procexp not found, retry in {} ms (attempt {}/5)", sleepMs, attempt + 1));
        Sleep(sleepMs);
    }
    
    g_terminationScheduled.store(false);
    return 0;
}

// Polling fallback: periodically check for ESO and terminate Procexp
DWORD WINAPI PollingWorker(LPVOID) {
    const auto logger = getLogger();
    logger->info(L"Polling fallback started");
    
    g_pollingActive.store(true);
    
    while (!bStop.load() && g_pollingActive.load()) {
        // Check if ESO is running
        bool esoRunning = false;
        HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{sizeof(PROCESSENTRY32W)};
            if (Process32FirstW(hSnapshot, &pe)) {
                do {
                    std::wstring exeFile(pe.szExeFile);
                    std::ranges::transform(exeFile, exeFile.begin(), ::toupper);
                    if (IsTargetProcess(exeFile)) {
                        esoRunning = true;
                        break;
                    }
                } while (Process32Next(hSnapshot, &pe));
            }
            CloseHandle(hSnapshot);
        }
        
        if (esoRunning) {
            // ESO is running, try to terminate Procexp
            HANDLE hSnapshot2 = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot2 != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32W pe{sizeof(PROCESSENTRY32W)};
                if (Process32FirstW(hSnapshot2, &pe)) {
                    do {
                        std::wstring exeFile(pe.szExeFile);
                        std::ranges::transform(exeFile, exeFile.begin(), ::toupper);
                        if (IsProcexpProcess(exeFile)) {
                            logger->info(std::format(L"Polling: Found Procexp {} (PID: {}), terminating", exeFile, pe.th32ProcessID));
                            TerminateProcessWithRetry(pe.th32ProcessID);
                            break;
                        }
                    } while (Process32NextW(hSnapshot2, &pe));
                }
                CloseHandle(hSnapshot2);
            }
        }
        
        // Poll every 2 seconds
        for (int i = 0; i < 20 && !bStop.load() && g_pollingActive.load(); ++i) {
            Sleep(100);
        }
    }
    
    logger->info(L"Polling fallback stopped");
    g_pollingThreadHandle = nullptr;
    return 0;
}

// Callback функция для обработки событий
void WINAPI ProcessEvent(PEVENT_RECORD psEventRecord) {
    if (bStop.load()) return;

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

        // ImageFileName is a wide string (WCHAR), so size is in bytes, divide by sizeof(WCHAR) for character count
        std::wstring processName(dwPropertySize / sizeof(WCHAR), L'\0');
        if (TdhGetProperty(
                psEventRecord,
                0,
                nullptr,
                1,
                &sPropertyDataDescriptor,
                dwPropertySize,
                PBYTE(processName.data())
            ) == ERROR_SUCCESS) {
            // Remove null terminator if present
            if (!processName.empty() && processName.back() == L'\0') {
                processName.pop_back();
            }
            std::ranges::transform(processName, processName.begin(), ::toupper);
            if (IsTargetProcess(processName)) {
                logger->info(L"Process started: " + processName);
                
                // Schedule single termination worker (deduplicated)
                if (!g_terminationScheduled.exchange(true)) {
                    CreateThread(nullptr, 0, TerminationWorker, nullptr, 0, nullptr);
                }
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

        bStop.store(false);
        
        // Start ETW processing thread
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
        
        // Start polling fallback thread
        g_pollingThreadHandle = CreateThread(nullptr, 0, PollingWorker, nullptr, 0, nullptr);
        if (g_pollingThreadHandle == nullptr) {
            logger->error(L"Watcher.start(). Failed to create polling thread");
        } else {
            logger->info(L"Watcher.start(). Polling fallback started");
        }

        logger->info(L"Watcher.start() exit");
        return true;
    }

    void stop() {
        const auto logger = getLogger();
        logger->info(L"Watcher.stop() enter");

        bStop.store(true);
        
        // Stop polling thread
        g_pollingActive.store(false);
        if (g_pollingThreadHandle != nullptr) {
            logger->info(L"Watcher.stop(). Waiting polling thread stop");
            WaitForSingleObject(g_pollingThreadHandle, 5000);
            CloseHandle(g_pollingThreadHandle);
            g_pollingThreadHandle = nullptr;
        }

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
