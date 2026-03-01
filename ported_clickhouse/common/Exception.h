#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace DB
{
class Exception : public std::runtime_error
{
public:
    explicit Exception(int code_, const std::string & message_)
        : std::runtime_error(message_), code(code_)
    {
    }

    template <typename... Args>
    Exception(int code_, const char * message_, Args &&...)
        : std::runtime_error(message_), code(code_)
    {
    }

    int codeValue() const { return code; }

private:
    int code;
};
}
