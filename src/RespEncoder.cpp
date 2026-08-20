#include "../include/RespEncoder.h"

std::string RespEncoder::encodeSimpleString(const std::string& val){
    return "+" + val + "\r\n";
}

std::string RespEncoder::encodeBulkString(const std::string& val){
    return "$" + std::to_string(val.size()) + "\r\n" + val + "\r\n";
}

std::string RespEncoder::encodeArray(const std::vector<std::string>& vals){
    std::string res = "*" + std::to_string(vals.size()) + "\r\n";

    for(const auto& val : vals){
        res += encodeBulkString(val);
    }

    return res;
}

std::string RespEncoder::encodeError(const std::string& val){
    return "-ERR " + val + "\r\n";
}

std::string RespEncoder::encodeNullBulkString(){
    return "$-1\r\n";
}

std::string RespEncoder::encodeInt(int val){
    return ":" + std::to_string(val) + "\r\n";
}