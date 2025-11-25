// logger.hpp
#pragma once

#include <string_view>

namespace postfga
{

    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    /// 공통 log 함수
    void log(LogLevel level, std::string_view msg);

    /// 편의 함수들
    inline void debug(std::string_view msg)
    {
        log(LogLevel::Debug, msg);
    }

    inline void info(std::string_view msg)
    {
        log(LogLevel::Info, msg);
    }

    inline void warning(std::string_view msg)
    {
        log(LogLevel::Warning, msg);
    }

    /// 주의: Error는 PostgreSQL ereport(ERROR)로 연결되면 longjmp 발생함.
    inline void error(std::string_view msg)
    {
        log(LogLevel::Error, msg);
    }

} // namespace postfga
