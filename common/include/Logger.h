#ifndef LOGGER_H
#define LOGGER_H
#include <combaseapi.h>
#include <memory>
#include <string_view>



enum class LogLevel {
    debug,
    info,
    warning,
    error,
};

interface ILogger {
    virtual ~ILogger() = default;

    void virtual init() = 0;
    void virtual log(std::wstring_view msg, LogLevel level) = 0;

    void virtual debug(std::wstring_view msg) = 0;

    void virtual info(std::wstring_view msg) = 0;

    void virtual warning(std::wstring_view msg) = 0;

    void virtual error(std::wstring_view msg) = 0;
};

class Logger final : public ILogger {
public:
    explicit Logger(std::wstring_view name);
    ~Logger() override;

    void log(std::wstring_view msg, LogLevel level) override;

    void debug(std::wstring_view msg) override;

    void info(std::wstring_view msg) override;

    void warning(std::wstring_view msg) override;

    void error(std::wstring_view msg) override;

    void init() override;

private:
    class LoggerPrivate;
    std::unique_ptr<LoggerPrivate> m_private;
};


#endif //LOGGER_H
