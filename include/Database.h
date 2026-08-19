#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>
#include <chrono>
#include "../include/WrongTypeError.h"

using RedisValue = std::variant<
    std::string,
    std::deque<std::string>,
    std::unordered_map<std::string,std::string>
>;

class Database{
    public:
    
    // Common commands
    bool flushAll(); 
    
    // key/val operations
    bool set(const std::string& key, const std::string& val);
    std::optional<std::string> get(const std::string& key)const;
    std::vector<std::string> keys()const;
    std::string type(const std::string& key)const;
    bool del(const std::string& key);
    bool rename(const std::string& oldKey, const std::string& newKey);
    void purgeExpired();
    void debugPrint() const;
    bool expire(const std::string& key, int seconds);


    private:
        std::unordered_map<std::string, RedisValue> m_data;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_expiryMap;

        template <typename T>
        T* findAs(const std::string& key);

        template <typename T>
        const T* findAs(const std::string& key)const;

        RedisValue* find(const std::string& key);
        const RedisValue* find(const std::string& key)const;
        bool isExpired(const std::string& key)const;
};

template <typename T>
T* Database::findAs(const std::string& key){
    auto entry = find(key);
    if(!entry) return nullptr;
    T* val = std::get_if<T>(entry);
    if(!val) throw WrongTypeError();
    return val;
}

template <typename T>
const T* Database::findAs(const std::string& key)const{
    auto entry = find(key);
    if(!entry) return nullptr;
    const T* val = std::get_if<T>(entry);
    if(!val) throw WrongTypeError();
    return val;
}