#include "../include/Database.h"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <cstdio>

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
    std::unique_lock lock(m_dbMutex);
    m_data.clear();
    return true;
}

// ===============persistence===============

namespace {
    // steady_clock has no fixed epoch, so a time_point from it can't be written to
    // disk and read back later

    long toEpochSeconds(const std::chrono::steady_clock::time_point& expiry,
                        const std::chrono::steady_clock::time_point& steadyNow,
                        const std::chrono::system_clock::time_point& sysNow){
        auto remaining = expiry - steadyNow;
        auto wallExpiry = sysNow + remaining;
        return static_cast<long>(
            std::chrono::duration_cast<std::chrono::seconds>(wallExpiry.time_since_epoch()).count()
        );
    }

    // nullopt means "no expiry" (seconds == 0) or "already expired" - both cases
    // where the caller should just drop the key instead of storing a time_point.
    std::optional<std::chrono::steady_clock::time_point> fromEpochSeconds(
            long seconds,
            const std::chrono::steady_clock::time_point& steadyNow,
            const std::chrono::system_clock::time_point& sysNow){
        if(seconds == 0) return std::nullopt;

        auto wallExpiry = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
        if(wallExpiry <= sysNow) return std::nullopt;

        return steadyNow + (wallExpiry - sysNow);
    }

    void writeValue(std::ofstream& out, const RedisValue& value){
        if(const auto* s = std::get_if<std::string>(&value)){
            out << *s << "\n";
        }else if(const auto* list = std::get_if<std::deque<std::string>>(&value)){
            out << list->size() << "\n";
            for(const auto& item : *list){
                out << item << "\n";
            }
        }else if(const auto* hash = std::get_if<std::unordered_map<std::string,std::string>>(&value)){
            out << hash->size() << "\n";
            for(const auto& [field, val] : *hash){
                out << field << "\n" << val << "\n";
            }
        }
    }

    RedisValue readValue(std::ifstream& in, const std::string& type){
        if(type == "string"){
            std::string val;
            std::getline(in, val);
            return val;
        }

        if(type == "list"){
            std::string countLine;
            std::getline(in, countLine);
            size_t n = std::stoul(countLine);

            std::deque<std::string> d;
            for(size_t i = 0; i < n; i++){
                std::string item;
                std::getline(in, item);
                d.push_back(item);
            }
            return d;
        }

        // type == "hash"
        std::string countLine;
        std::getline(in, countLine);
        size_t n = std::stoul(countLine);

        std::unordered_map<std::string, std::string> h;
        for(size_t i = 0; i < n; i++){
            std::string field, val;
            std::getline(in, field);
            std::getline(in, val);
            h[field] = val;
        }
        return h;
    }
}

bool Database::save(const std::string& path)const{
    std::shared_lock lock(m_dbMutex);

    std::string tmpPath = path + ".tmp";
    std::ofstream out(tmpPath);
    if(!out) return false;

    out << "KVDB1\n";

    size_t liveCount = 0;
    for(const auto& [key, value] : m_data){
        if(!isExpired(key)) liveCount++;
    }
    out << liveCount << "\n";

    auto steadyNow = std::chrono::steady_clock::now();
    auto sysNow = std::chrono::system_clock::now();

    for(const auto& [key, value] : m_data){
        if(isExpired(key)) continue;

        long expirySeconds = 0;
        auto expIt = m_expiryMap.find(key);
        if(expIt != m_expiryMap.end()){
            expirySeconds = toEpochSeconds(expIt->second, steadyNow, sysNow);
        }

        std::string type;
        if(std::holds_alternative<std::string>(value)) type = "string";
        else if(std::holds_alternative<std::deque<std::string>>(value)) type = "list";
        else type = "hash";

        out << type << "\n" << key << "\n" << expirySeconds << "\n";
        writeValue(out, value);
    }

    out.close();
    if(!out) return false;

    if(std::rename(tmpPath.c_str(), path.c_str()) != 0){
        return false;
    }

    return true;
}

bool Database::load(const std::string& path){
    std::ifstream in(path);
    if(!in) return false;

    std::string magic;
    std::getline(in, magic);
    if(magic != "KVDB1") return false;

    std::string countLine;
    std::getline(in, countLine);
    size_t count = std::stoul(countLine);

    std::unordered_map<std::string, RedisValue> newData;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> newExpiry;

    auto steadyNow = std::chrono::steady_clock::now();
    auto sysNow = std::chrono::system_clock::now();

    for(size_t i = 0; i < count; i++){
        std::string type, key, expiryLine;
        std::getline(in, type);
        std::getline(in, key);
        std::getline(in, expiryLine);
        long expirySeconds = std::stol(expiryLine);

        if(type != "string" && type != "list" && type != "hash"){
            return false; // unknown type, treat the file as corrupt
        }

        RedisValue value = readValue(in, type);
        if(!in) return false;

        auto expiryPoint = fromEpochSeconds(expirySeconds, steadyNow, sysNow);
        if(expirySeconds != 0 && !expiryPoint.has_value()){
            continue; // already expired, drop it
        }

        newData[key] = std::move(value);
        if(expiryPoint.has_value()){
            newExpiry[key] = *expiryPoint;
        }
    }

    std::unique_lock lock(m_dbMutex);
    m_data = std::move(newData);
    m_expiryMap = std::move(newExpiry);

    return true;
}

// ===============Key/Value(string) Operations===============

bool Database::set(const std::string& key, const std::string& val){
    std::unique_lock lock(m_dbMutex);
    m_data[key] = val;
    return true;
}

std::optional<std::string> Database::get(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::string>(key);
    if(it){
        return *it;
    }
    return std::nullopt;
}

std::vector<std::string> Database::keys()const{
    std::shared_lock lock(m_dbMutex);
    std::vector<std::string> allKeys;
    for(const auto& key : m_data){
        if(!isExpired(key.first))
            allKeys.push_back(key.first);
    }
    return allKeys;
}

std::string Database::type(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
    bool wasLive = find(key) != nullptr;
    m_data.erase(key);
    m_expiryMap.erase(key);
    return wasLive;
}

bool Database::rename(const std::string& oldKey, const std::string& newKey){
    std::unique_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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

void Database::handleCleanup(){
    while(m_running){
        std::this_thread::sleep_for(std::chrono::seconds(1));
        purgeExpired();
    }
}

bool Database::expire(const std::string& key, int seconds){
    std::unique_lock lock(m_dbMutex);
    if(find(key) == nullptr){
        return false;
    }
    
    m_expiryMap[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
}

//===============list operations===============

size_t Database::lpush(const std::string& key, const std::vector<std::string>& values){
    std::unique_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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

size_t Database::llen(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::deque<std::string>>(key);

    if(!it){
        return 0;
    }
    return it->size();
}

std::optional<std::string> Database::lindex(const std::string& key, int index)const{
    std::shared_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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
    std::unique_lock lock(m_dbMutex);
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

std::vector<std::string> Database::lrange(const std::string& key, int start, int stop)const{
    std::shared_lock lock(m_dbMutex);
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

//===============hash operations===============

size_t Database::hset(const std::string& key, const std::vector<std::string>& values){
    std::unique_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);
    size_t newEntries = 0;

    if(!it){
        m_data[key] = std::unordered_map<std::string, std::string>();
        it = findAs<std::unordered_map<std::string,std::string>>(key);
    }


    for(size_t i = 0; i + 1 < values.size(); i += 2){
        auto [_, isInserted] = it->insert_or_assign(values[i], values[i+1]);
        if(isInserted){
            newEntries++;
        }
    }

    return newEntries;
}

std::optional<std::string> Database::hget(const std::string& key, const std::string& field)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);

    if(!it){
        return std::nullopt;
    }

    auto f = it->find(field);
    if(f == it->end()){
        return std::nullopt;
    }
    
    return f->second;
}

bool Database::hexists(const std::string& key, const std::string& field)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);

    if(!it){
        return 0;
    }

    auto found = it->find(field);

    return found != it->end();
}

size_t Database::hdel(const std::string& key, const std::vector<std::string>& fields){
    std::unique_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);
    size_t deletedCount = 0;

    if(!it){
        return deletedCount;
    }

    for(const auto& field : fields){
        deletedCount += it->erase(field);
    }

    if(it->empty()){
        m_data.erase(key);
        m_expiryMap.erase(key);
    }

    return deletedCount;
}

size_t Database::hlen(const std::string& key){
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);

    if(!it){
        return 0;
    }

    return it->size();
}

std::vector<std::string> Database::hkeys(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);
    std::vector<std::string> res;

    if(!it){
        return res;
    }

    for(const auto& [key, _]: *it){
        res.push_back(key);
    }
    return res;
}

std::vector<std::string> Database::hvals(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);
    std::vector<std::string> res;

    if(!it){
        return res;
    }

    for(const auto& [_, val]: *it){
        res.push_back(val);
    }
    return res;
}

std::vector<std::string> Database::hgetall(const std::string& key)const{
    std::shared_lock lock(m_dbMutex);
    auto it = findAs<std::unordered_map<std::string,std::string>>(key);
    std::vector<std::string> res;

    if(!it){
        return res;
    }
    
    for(const auto& [key, val]: *it){
        res.push_back(key);
        res.push_back(val);
    }
    return res;
}


Database::Database(){
    m_cleanupThread = std::thread([this](){
        handleCleanup();
    });
}

Database::~Database(){
    m_running = false;
    m_cleanupThread.join();
}