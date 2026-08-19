#pragma once

#include <string>
#include <vector>
#include <optional>

class RespParser{
    public:
        std::optional<std::vector<std::string>> tryParseCommand();
        void feed(const char* bytes, size_t len);

    private:
        std::string m_buffer;

        std::optional<std::string> readLine(size_t& pos);
        std::optional<std::string> parseBulkString(size_t& pos);
        std::optional<std::vector<std::string>> parseArray(size_t& pos);
};