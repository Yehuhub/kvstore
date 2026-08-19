#pragma once

#include <string>
#include <vector>
#include <optional>
#include "../include/Command.h"

class RespParser{
    public:
        std::optional<Command> tryParseCommand();
        void feed(const char* bytes, size_t len);

    private:
        std::string m_buffer;

        std::optional<std::string> readLine(size_t& pos);
        std::optional<std::string> parseBulkString(size_t& pos);
        std::optional<std::vector<std::string>> parseArray(size_t& pos);
};