#include "../include/Database.h"
#include <iostream>
#include <algorithm>

RedisValue* Database::find(const std::string& key){
    auto it = m_data.find(key);
    if(it == m_data.end() || isExpired(key)) return nullptr;
    return &it->second;
}

const RedisValue* Database::find(const std::string& key)const{
    auto it = m_data.find(key);
    if(it == m_data.end() || isExpired(key)) return nullptr;
    return &it->second;
}

bool Database::isExpired(const std::string& key)const{
    auto it = m_expiryMap.find(key);
    return it != m_expiryMap.end() && it->second <= std::chrono::steady_clock::now();
}

// ===============common commands===============
bool Database::flushAll(){
    m_data.clear();
    return true;
}

// ===============Key/Value(string) Operations===============

bool Database::set(const std::string& key, const std::string& val){
    m_data[key] = val;
    return true;
}

std::optional<std::string> Database::get(const std::string& key)const{
    auto it = findAs<std::string>(key);
    if(it){
        return *it;
    }
    return std::nullopt;
}

std::vector<std::string> Database::keys()const{
    std::vector<std::string> allKeys;
    for(const auto& key : m_data){
        if(!isExpired(key.first))
            allKeys.push_back(key.first);
    }
    return allKeys;
}

std::string Database::type(const std::string& key)const{
    auto val = find(key);
    if(val == nullptr){
        return "none";
    }else if (std::holds_alternative<std::string>(*val)){
        return "string";
    }else if(std::holds_alternative<std::deque<std::string>>(*val)){
        return "list";
    }else{
        return "hash";
    }
}

bool Database::del(const std::string& key){
    bool wasLive = find(key) != nullptr;
    m_data.erase(key);
    m_expiryMap.erase(key);
    return wasLive;
}

bool Database::rename(const std::string& oldKey, const std::string& newKey){
    //move the value to the new key
    if(find(oldKey) == nullptr){
        return false;
    }
    auto node = m_data.extract(oldKey);
    node.key() = newKey;

    m_data.erase(newKey);
    m_data.insert(std::move(node));

    //move expiry to the new key
    m_expiryMap.erase(newKey);
    auto expIt = m_expiryMap.find(oldKey);
    if(expIt != m_expiryMap.end()){
        auto expNode = m_expiryMap.extract(expIt);
        expNode.key() = newKey;
        m_expiryMap.insert(std::move(expNode));
    }

    return true;
}

void Database::purgeExpired(){
    auto now = std::chrono::steady_clock::now();
    for(auto it = m_expiryMap.begin(); it != m_expiryMap.end(); ){
        if(now > it->second){
            m_data.erase(it->first);
            it = m_expiryMap.erase(it);
        }else{
            it++;
        }
    }
}

bool Database::expire(const std::string& key, int seconds){
    if(find(key) == nullptr){
        return false;
    }
    
    m_expiryMap[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
}

//===============list operations===============

size_t Database::lpush(const std::string& key, const std::vector<std::string>& values){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        std::deque<std::string> deque;
        for(const auto& value : values){
            deque.push_front(value);
        }
        m_data[key] = std::move(deque);
        return values.size();
    }
    for(const auto& value : values){
            it->push_front(value);
    }
    return it->size();
}

size_t Database::rpush(const std::string& key, const std::vector<std::string>& values){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        std::deque<std::string> deque;
        for(const auto& value : values){
            deque.push_back(value);
        }
        m_data[key] = std::move(deque);
        return values.size();
    }
    for(const auto& value : values){
            it->push_back(value);
    }
    return it->size();
}

std::optional<std::string> Database::lpop(const std::string& key){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it || it->empty()){
        return std::nullopt;
    }

    auto val = it->front();
    it->pop_front();
    if(it->empty()){
        m_data.erase(key);
        m_expiryMap.erase(key);
    }
    return val;
}

std::optional<std::string> Database::rpop(const std::string& key){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it || it->empty()){
        return std::nullopt;
    }

    auto val = it->back();
    it->pop_back();
    if(it->empty()){
        m_data.erase(key);
        m_expiryMap.erase(key);
    }
    return val;
}

size_t Database::llen(const std::string& key){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        return 0;
    }
    return it->size();
}

std::optional<std::string> Database::lindex(const std::string& key, int index){
    auto it = findAs<std::deque<std::string>>(key);
    
    if(!it){
        return std::nullopt;
    }

    if(index < 0){
        index += static_cast<int>(it->size());
    }
    if(index < 0 || index >= static_cast<int>(it->size())){
        return std::nullopt;
    }

    return (*it)[index];
}

bool Database::lset(const std::string& key, int index, const std::string& val){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        return false;
    }

    if(index < 0){
        index += static_cast<int>(it->size());
    }
    if(index < 0 || index >= static_cast<int>(it->size())){
        return false;
    }

    (*it)[index] = val;
    return true;
}

size_t Database::lrem(const std::string& key, int count, const std::string& val){
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        return 0;
    }
    
    int erasedAmount = 0;
     if (count >= 0) {
        for (auto i = it->begin(); i != it->end();) {
            if (*i == val && (count == 0 || erasedAmount < count)) {
                i = it->erase(i);
                ++erasedAmount;
            } else {
                ++i;
            }
        }
    }else {
        int limit = -count;
        for (auto i = it->end(); i != it->begin();) {
            --i;
            if (*i == val && erasedAmount < limit) {
                i = it->erase(i);
                ++erasedAmount;
                if (i == it->end()) {
                    break;
                }
            }
        }
    }

    if (it->empty()) {
        m_data.erase(key);
        m_expiryMap.erase(key);
    }

    return erasedAmount;
}

std::vector<std::string> Database::lrange(const std::string& key, int start, int stop){
    auto it = findAs<std::deque<std::string>>(key);
    std::vector<std::string> res;
    
    if(!it){
        return res;
    }
    int arraySize = static_cast<int>(it->size());
    
    //normalize indices
    if(start < 0){
        start + arraySize < 0 ? start = 0 : start += arraySize;
    }
    if(stop < 0){
        stop += arraySize;
    }
    if(stop >= arraySize){
        stop = arraySize - 1;
    }
    
    if(start <= stop && start < arraySize){
        for(auto i = start; i <= stop; i++){
            res.push_back((*it)[i]);
        }
    }
    
    return res;
}

//------debug and printing--------

void Database::debugPrint() const {
    for (const auto& [key, value] : m_data) {
        std::cout << key << " = ";

        std::visit([](const auto& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::string>) {
                std::cout << "\"" << v << "\"";
            } else if constexpr (std::is_same_v<T, std::deque<std::string>>) {
                for (size_t i = 0; i < v.size(); ++i) {
                    std::cout << "\"" << v[i] << "\"";
                    if (i + 1 < v.size()) std::cout << ", ";
                }
                std::cout << "]";
            } else {
                std::cout << "{";
                bool first = true;
                for (const auto& [field, val] : v) {
                    if (!first) std::cout << ", ";
                    std::cout << field << ": \"" << val << "\"";
                    first = false;
                }   
                std::cout << "}";
            }   
        }, value);

        auto expIt = m_expiryMap.find(key);
        if (expIt != m_expiryMap.end()) {
            if (isExpired(key)) {
                std::cout << " [EXPIRED, not yet purged]";
            } else {
                auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                    expIt->second - std::chrono::steady_clock::now()).count();
                std::cout << " (ttl: " << remaining << "s)";
            }
        }

        std::cout << "\n";
    }
}