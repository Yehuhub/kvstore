#include "../include/CommandHandler.h"
#include "../include/RespEncoder.h"
#include <stdexcept>

CommandHandler::CommandHandler(Database& db)
    : m_db(db),
      m_handlers{
        //common
        {"PING", &CommandHandler::handlePing},
        {"ECHO", &CommandHandler::handleEcho},
        {"FLUSHALL", &CommandHandler::handleFlushAll},
        // kv
        {"SET", &CommandHandler::handleSet},
        {"GET", &CommandHandler::handleGet},
        {"KEYS", &CommandHandler::handleKeys},
        {"TYPE", &CommandHandler::handleType},
        {"DEL", &CommandHandler::handleDel},
        {"RENAME", &CommandHandler::handleRename},
        {"EXPIRE", &CommandHandler::handleExpire},
        // list
        {"LPUSH", &CommandHandler::handlePush},
        {"RPUSH", &CommandHandler::handlePush},
        {"LPOP", &CommandHandler::handlePop},
        {"RPOP", &CommandHandler::handlePop},
        {"LLEN", &CommandHandler::handleLlen},
        {"LINDEX", &CommandHandler::handleLindex},
        {"LSET", &CommandHandler::handleLset},
        {"LREM", &CommandHandler::handleLrem},
        {"LRANGE", &CommandHandler::handleLrange},
        // hash
        {"HSET", &CommandHandler::handleHset},
        {"HGET", &CommandHandler::handleHget},
        {"HEXISTS", &CommandHandler::handleHexists},
        {"HDEL", &CommandHandler::handleHdel},
        {"HLEN", &CommandHandler::handleHlen},
        {"HKEYS", &CommandHandler::handleHkeys},
        {"HVALS", &CommandHandler::handleHvals},
        {"HGETALL", &CommandHandler::handleHgetall}

      }
{};

std::string CommandHandler::processCommand(const Command& cmd){
    auto it = m_handlers.find(cmd.m_name);
    if(it == m_handlers.end()){
        return RespEncoder::encodeError("unknown command");
    }

    return (this->*(it->second))(cmd);

}

std::string wrongNumOfArguments(const std::string& cmdName){
    return RespEncoder::encodeError("wrong number of arguments for '" + cmdName + "' command");
}

//===========Command Handler Functions===========

//-------Common Commands-------
std::string CommandHandler::handlePing(const Command& cmd){
    return RespEncoder::encodeSimpleString("PONG");
}

std::string CommandHandler::handleEcho(const Command& cmd){
    if (cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    return RespEncoder::encodeBulkString(cmd.m_args[0]);
}

std::string CommandHandler::handleFlushAll(const Command& cmd){
    if(!cmd.m_args.empty()){
        return wrongNumOfArguments(cmd.m_name);
    }
    m_db.flushAll();
    return RespEncoder::encodeSimpleString("OK");
}


//-------KV Operations-------
std::string CommandHandler::handleSet(const Command& cmd){
    if(cmd.m_args.empty() || cmd.m_args.size() > 2){
        return wrongNumOfArguments(cmd.m_name);
    }
    m_db.set(cmd.m_args[0], cmd.m_args[1]);
    return RespEncoder::encodeSimpleString("OK");
}

std::string CommandHandler::handleGet(const Command& cmd){
    if(cmd.m_args.empty() || cmd.m_args.size() > 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    auto val = m_db.get(cmd.m_args[0]);
    if(val == std::nullopt){
        return RespEncoder::encodeNullBulkString();
    }
    return RespEncoder::encodeBulkString(*val);
}

std::string CommandHandler::handleKeys(const Command& cmd){
    if(!cmd.m_args.empty()){
        return wrongNumOfArguments(cmd.m_name);
    }
    auto allKeys = m_db.keys();

    return RespEncoder::encodeArray(allKeys);

}

std::string CommandHandler::handleType(const Command& cmd){
    if(cmd.m_args.empty() || cmd.m_args.size() > 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    return RespEncoder::encodeSimpleString(m_db.type(cmd.m_args[0]));
}

std::string CommandHandler::handleDel(const Command& cmd){
    if(cmd.m_args.empty()){
        return wrongNumOfArguments(cmd.m_name);
    }
    int deletedCount = 0;

    for(const auto& key : cmd.m_args){
        auto res = m_db.del(key);
        if(res) deletedCount++;
    }
    return RespEncoder::encodeInt(deletedCount);
}

std::string CommandHandler::handleRename(const Command& cmd){
    if(cmd.m_args.size() != 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto success = m_db.rename(cmd.m_args[0], cmd.m_args[1]);
    if(!success){
        return RespEncoder::encodeError("no such key");
    }
    return RespEncoder::encodeSimpleString("OK");
}

std::string CommandHandler::handleExpire(const Command& cmd){
    if(cmd.m_args.size() != 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    try{
        auto success = m_db.expire(cmd.m_args[0], std::stoi(cmd.m_args[1]));
        return RespEncoder::encodeInt(success ? 1 : 0);
    }catch(const std::invalid_argument&){
        return RespEncoder::encodeError("invalid expire time in 'expire' command");
    }catch(const std::out_of_range&){
        return RespEncoder::encodeError("invalid expire time in 'expire' command");
    }

}

//-------List Operations-------

std::string CommandHandler::handlePush(const Command& cmd){
    if(cmd.m_args.size() <= 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    
    auto key = cmd.m_args[0];
    std::vector<std::string> values(cmd.m_args.begin() + 1, cmd.m_args.end());
    
    
    auto length = cmd.m_name == "LPUSH" ?
    m_db.lpush(key, values) :
    m_db.rpush(key, values);
    
    return RespEncoder::encodeInt(length);
}


std::string CommandHandler::handlePop(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    
    auto key = cmd.m_args[0];
    auto res = cmd.m_name == "LPOP" ? m_db.lpop(key) : m_db.rpop(key);
    
    return res.has_value() ?
    RespEncoder::encodeBulkString(*res) :
    RespEncoder::encodeNullBulkString(); 
}

std::string CommandHandler::handleLlen(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    
    auto key = cmd.m_args[0];
    auto res = m_db.llen(key);
    return RespEncoder::encodeInt(res);
}
std::string CommandHandler::handleLindex(const Command& cmd){
    if(cmd.m_args.size() != 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    try{

        auto key = cmd.m_args[0];
        auto index = std::stoi(cmd.m_args[1]);
        auto res = m_db.lindex(key, index);

        return res.has_value() ?
            RespEncoder::encodeBulkString(*res) :
            RespEncoder::encodeNullBulkString();
    
    }catch(const std::invalid_argument&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }catch(const std::out_of_range&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }

}

std::string CommandHandler::handleLset(const Command& cmd){
    if(cmd.m_args.size() != 3){
        return wrongNumOfArguments(cmd.m_name);
    }

    try{
        auto key = cmd.m_args[0];
        auto index = std::stoi(cmd.m_args[1]);
        auto val = cmd.m_args[2];

        auto res = m_db.lset(key, index, val);

        return res ?
            RespEncoder::encodeSimpleString("OK"):
            RespEncoder::encodeError("no such key or index out of range");

    }catch(const std::invalid_argument&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }catch(const std::out_of_range&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }
}

std::string CommandHandler::handleLrem(const Command& cmd){
    if(cmd.m_args.size() != 3){
        return wrongNumOfArguments(cmd.m_name);
    }

    try{
        auto key = cmd.m_args[0];
        auto count = std::stoi(cmd.m_args[1]);
        auto val = cmd.m_args[2];
        
        auto res = m_db.lrem(key, count, val);
        return RespEncoder::encodeInt(res);
        
    }catch(const std::invalid_argument&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }catch(const std::out_of_range&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }
}

std::string CommandHandler::handleLrange(const Command& cmd){
    if(cmd.m_args.size() != 3){
        return wrongNumOfArguments(cmd.m_name);
    }

    try{
        auto key = cmd.m_args[0];
        auto start = std::stoi(cmd.m_args[1]);
        auto stop = std::stoi(cmd.m_args[2]);

        auto res = m_db.lrange(key, start, stop);
        return RespEncoder::encodeArray(res);

    }catch(const std::invalid_argument&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }catch(const std::out_of_range&){
        return RespEncoder::encodeError("value is not an integer or out of range");
    }
}

//-------Hash Operations-------

std::string CommandHandler::handleHset(const Command& cmd){
    if(cmd.m_args.size() < 3 || cmd.m_args.size() % 2 == 0){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];
    std::vector<std::string> values(cmd.m_args.begin() + 1, cmd.m_args.end());

    auto res = m_db.hset(key, values);
    return RespEncoder::encodeInt(res);
}

std::string CommandHandler::handleHget(const Command& cmd){
    if(cmd.m_args.size() != 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];
    auto field = cmd.m_args[1];

    auto res = m_db.hget(key, field);

    return res.has_value() ?
        RespEncoder::encodeBulkString(*res) :
        RespEncoder::encodeNullBulkString();
}

std::string CommandHandler::handleHdel(const Command& cmd){
    if(cmd.m_args.size() < 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];
    std::vector<std::string> fields(cmd.m_args.begin() + 1, cmd.m_args.end());

    auto res = m_db.hdel(key, fields);
    return RespEncoder::encodeInt(res);

}

std::string CommandHandler::handleHlen(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];
    
    auto res = m_db.hlen(key);
    return RespEncoder::encodeInt(res);
}

std::string CommandHandler::handleHkeys(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];

    auto res = m_db.hkeys(key);
    return RespEncoder::encodeArray(res);
}

std::string CommandHandler::handleHvals(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];

    auto res = m_db.hvals(key);
    return RespEncoder::encodeArray(res);
}

std::string CommandHandler::handleHgetall(const Command& cmd){
    if(cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];

    auto res = m_db.hgetall(key);
    return RespEncoder::encodeArray(res);
}

std::string CommandHandler::handleHexists(const Command& cmd){
    if(cmd.m_args.size() != 2){
        return wrongNumOfArguments(cmd.m_name);
    }

    auto key = cmd.m_args[0];
    auto field = cmd.m_args[1];

    auto res = m_db.hexists(key, field);
    return RespEncoder::encodeInt(res);
}

// std::string CommandHandler::handle(const Command& cmd)