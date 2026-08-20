#include "../include/CommandHandler.h"
#include "../include/RespEncoder.h"
#include <stdexcept>

CommandHandler::CommandHandler(Database& db)
    : m_db(db),
      m_handlers{
        {"PING", &CommandHandler::handlePing},
        {"ECHO", &CommandHandler::handleEcho},
        {"FLUSHALL", &CommandHandler::handleFlushAll},
        {"SET", &CommandHandler::handleSet},
        {"GET", &CommandHandler::handleGet},
        {"KEYS", &CommandHandler::handleKeys},
        {"TYPE", &CommandHandler::handleType},
        {"DEL", &CommandHandler::handleDel},
        {"RENAME", &CommandHandler::handleRename},
        {"EXPIRE", &CommandHandler::handleExpire}
      }
{};

std::string wrongNumOfArguments(const std::string& cmdName){
    return RespEncoder::encodeError("wrong number of arguments for '" + cmdName + "' command\r\n");
}

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
    }catch(const std::out_of_range*){
        return RespEncoder::encodeError("invalid expire time in 'expire' command");
    }

}

std::string CommandHandler::processCommand(const Command& cmd){
    auto it = m_handlers.find(cmd.m_name);
    if(it == m_handlers.end()){
        return RespEncoder::encodeError("unknown command");
    }

    return (this->*(it->second))(cmd);

}
