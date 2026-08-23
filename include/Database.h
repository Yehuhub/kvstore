#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <optional>
#include <chrono>
#include <vector>
#include <string>
#include <shared_mutex>
#include <mutex>
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
    bool expire(const std::string& key, int seconds);

    // list operations
    size_t lpush(const std::string& key, const std::vector<std::string>& values);
    size_t rpush(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::string> lpop(const std::string& key);
    std::optional<std::string> rpop(const std::string& key);
    size_t llen(const std::string& key)const;
    std::optional<std::string> lindex(const std::string& key, int index)const;
    bool lset(const std::string& key, int index, const std::string& val);
    size_t lrem(const std::string& key, int count, const std::string& val);
    std::vector<std::string> lrange(const std::string& key, int start, int stop)const;

    // hash operations
    size_t hset(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::string> hget(const std::string& key, const std::string& field)const;
    bool hexists(const std::string& key, const std::string& field)const;
    size_t hdel(const std::string& key, const std::vector<std::string>& fields);
    size_t hlen(const std::string& key);
    std::vector<std::string> hkeys(const std::string& key)const;
    std::vector<std::string> hvals(const std::string& key)const;
    std::vector<std::string> hgetall(const std::string& key)const;

    // utility functions
    void purgeExpired();


    private:
        std::unordered_map<std::string, RedisValue> m_data;
        std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_expiryMap;

        mutable std::shared_mutex m_dbMutex;

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