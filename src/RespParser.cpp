#include "../include/RespParser.h"
#include <stdexcept>

void RespParser::feed(const char* bytes, size_t len){
    m_buffer.append(bytes, len);
}


std::optional<std::string> RespParser::readLine(size_t& pos) {
        size_t crlf = m_buffer.find("\r\n", pos);
        if (crlf == std::string::npos) return std::nullopt;
        std::string line = m_buffer.substr(pos, crlf - pos);
        pos = crlf + 2;
        return line;
}


std::optional<std::string> RespParser::parseBulkString(size_t& pos) {
        auto line = readLine(pos);
        if (!line) return std::nullopt;
        if ((*line)[0] != '$')
            throw std::runtime_error("protocol error: expected '$'");

        int len = std::stoi(line->substr(1));
        if (len < 0 || len > 512*1024*1024) 
            throw std::runtime_error("Protocol error: invalid bulk length");

        if (m_buffer.size() < pos + len + 2) return std::nullopt;

        std::string content = m_buffer.substr(pos, len);
        pos += len + 2;
        return content;
}

std::optional<std::vector<std::string>> RespParser::parseArray(size_t& pos) {
        auto line = readLine(pos);               // e.g. "*2"
        if (!line) return std::nullopt;
        if ((*line)[0] != '*')
            throw std::runtime_error("protocol error: expected '*'");

        int count = std::stoi(line->substr(1));
        
        if (count < 0 || count > 1024*1024) 
            throw std::runtime_error("Protocol error: invalid multibulk length");

        std::vector<std::string> args;
        args.reserve(count);
        for (int i = 0; i < count; i++) {
            auto item = parseBulkString(pos);
            if (!item) return std::nullopt;      // incomplete mid-array
            args.push_back(*item);
        }
        return args;
}


std::optional<Command> RespParser::tryParseCommand() {
        size_t pos = 0;
        auto result = parseArray(pos);
        
        if (!result) return std::nullopt;      // incomplete — buffer untouched

        m_buffer.erase(0, pos);           // success — drop consumed bytes

        Command cmd;
        cmd.m_name = (*result)[0];
        std::transform(
            cmd.m_name.begin(),
            cmd.m_name.end(),
            cmd.m_name.begin(),
            [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            }
        );

        for (size_t i = 1; i < result->size(); ++i){
            cmd.m_args.push_back((*result)[i]);
        }

        return cmd;
}
