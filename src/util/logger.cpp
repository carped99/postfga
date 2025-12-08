// logger.cpp

extern "C"
{
#include "postgres.h"
#include "utils/elog.h"
}

#include "logger.hpp"
#include <string> // std::string

namespace fga::util
{
    static int
    to_pg_level(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Debug:
            return DEBUG1;
        case LogLevel::Info:
            return LOG;
        case LogLevel::Warning:
            return WARNING;
        case LogLevel::Error:
            return ERROR;
        }

        /* 방어용: 기본은 LOG */
        return LOG;
    }

    void log(LogLevel level, std::string_view msg)
    {
        const int elevel = to_pg_level(level);

        // ereport는 C 스타일 문자열이 필요하니 std::string으로 한 번 감싸줌
        std::string s(msg);

        ereport(elevel,
                errmsg("%s", s.c_str()));
    }

} // namespace fga
