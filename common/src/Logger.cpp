#define __STDC_WANT_LIB_EXT1__ 1 // NOLINT(*-reserved-identifier)
#include "../include/Logger.h"
#include <filesystem>
#include <fstream>
#include <ctime>
#include <mutex>

#include "../include/consts.h"


class Logger::LoggerPrivate final : public ILogger {
public:
    explicit Logger::LoggerPrivate(std::wstring_view name)
        : ILogger(),
          m_timeStamp(26, L'\0') {
        OutputDebugStringW((m_debugPrefix + L"Create log").c_str());
        const auto logPath = std::filesystem::temp_directory_path() / SERVICE_NAME;
        OutputDebugStringW((m_debugPrefix + L"LogPath: " + logPath.wstring()).c_str());
        if (!std::filesystem::exists(logPath)) {
            if (std::filesystem::create_directories(logPath)) {
                OutputDebugStringW((m_debugPrefix + L"Log directory created").c_str());
            } else {
                OutputDebugStringW((m_debugPrefix + L"Log directory create FAILED").c_str());
            }
        }
        m_logFile = logPath / (std::wstring(name) + L".log");
        OutputDebugStringW((m_debugPrefix + L"LogFile: " + m_logFile.wstring()).c_str());

        const auto now = std::time(nullptr);
        localtime_s(&m_logDate, &now);
    }

    void log(std::wstring_view message, LogLevel level) override {
        OutputDebugStringW((m_debugPrefix + std::wstring(message)).c_str());

        const auto now = std::time(nullptr);
        std::tm tm{};
        if (localtime_s(&tm, &now) == 0) {
            const auto maxSize = m_timeStamp.capacity();
            const auto size = wcsftime(m_timeStamp.data(), maxSize, L"%F %T", &tm);

            m_timeStamp.resize(size);
        }
        std::wstringstream stream{};
        stream << L'[' << m_timeStamp << L"] ";
        if (level >= LogLevel::debug and level <= LogLevel::error) {
            stream << m_aLevelStrings[static_cast<int>(level)] << ": ";
        }
        stream << message;

        _log(stream.str());
    }

    void debug(std::wstring_view msg) override {
        log(msg, LogLevel::debug);
    }

    void info(std::wstring_view msg) override {
        log(msg, LogLevel::info);
    }

    void warning(std::wstring_view msg) override {
        log(msg, LogLevel::warning);
    }

    void error(std::wstring_view msg) override {
        log(msg, LogLevel::error);
    }

    void init() override {
        _log(L"[TimeStamp] [LogLevel] [Message]", std::ios_base::out | std::ios_base::trunc);
    };

private:
    std::filesystem::path m_logFile;
    std::wstring m_timeStamp;
    std::vector<std::wstring> m_aLevelStrings{L"Debug", L"Info", L"Warning", L"Error"};
    std::mutex m_mutex;
    std::tm m_logDate{0};
    std::wstring m_debugPrefix{DEBUG_PREFIX};

    void _log(std::wstring_view msg, std::ios_base::openmode openmode = std::ios_base::app | std::ios_base::out) {
        std::lock_guard guard(m_mutex);


        if ((openmode & std::ios_base::app) == std::ios_base::app) {
            const auto now = std::time(nullptr);
            std::tm tm{0};
            localtime_s(&tm, &now);
            if (m_logDate.tm_yday != tm.tm_yday || m_logDate.tm_year != tm.tm_year) {
                openmode = std::ios_base::out | std::ios_base::trunc;
            }
        }
        auto stream = std::wofstream(m_logFile, openmode);
        stream << msg << std::endl;
        stream.flush();
        stream.close();
    }
};


Logger::~Logger() = default;

Logger::Logger(std::wstring_view name)
    : ILogger(),
      m_private(std::make_unique<LoggerPrivate>(name)) {
}

void Logger::log(std::wstring_view msg, LogLevel level) {
    m_private->log(msg, level);
}

auto Logger::debug(std::wstring_view msg) -> void {
    m_private->debug(msg);
}

void Logger::info(std::wstring_view msg) {
    m_private->info(msg);
}

void Logger::warning(std::wstring_view msg) {
    m_private->warning(msg);
}

void Logger::error(std::wstring_view msg) {
    m_private->error(msg);
}

void Logger::init() {
    m_private->init();
}
