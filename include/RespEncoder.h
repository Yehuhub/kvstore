#pragma once
#include <string>
#include <vector>

class RespEncoder{
    public:
        static std::string encodeSimpleString(const std::string& val);
        static std::string encodeBulkString(const std::string& val);
        static std::string encodeNullBulkString();
        static std::string encodeArray(const std::vector<std::string>& vals);
        static std::string encodeError(const std::string& val);
        static std::string encodeInt(int val);
};