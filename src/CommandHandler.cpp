#include "../include/CommandHandler.h"

CommandHandler::CommandHandler(Database& db)
    : m_db(db),
      m_handlers{
        {"PING", &CommandHandler::handlePing},
        {"ECHO", &CommandHandler::handleEcho},
        {"FLUSHALL", &CommandHandler::handleFlushAll},
        {"SET", &CommandHandler::handleSet},
        {"GET", &CommandHandler::handleGet}
      }
{};

std::string wrongNumOfArguments(const std::string& cmdName){
    return "-ERR wrong number of arguments for '" + cmdName + "' command\r\n";
}

std::string CommandHandler::handlePing(const Command& cmd){
    return "+PONG\r\n";
}

std::string CommandHandler::handleEcho(const Command& cmd){
    if (cmd.m_args.size() != 1){
        return wrongNumOfArguments(cmd.m_name);
    }

    const auto& message = cmd.m_args[0];

    return "$" + std::to_string(message.size()) +
           "\r\n" + message + "\r\n";
}

std::string CommandHandler::handleFlushAll(const Command& cmd){
    if(!cmd.m_args.empty()){
        return wrongNumOfArguments(cmd.m_name);
    }
    m_db.flushAll();
    return "+OK\r\n";
}

std::string CommandHandler::handleSet(const Command& cmd){
    if(cmd.m_args.empty() || cmd.m_args.size() > 2){
        return wrongNumOfArguments(cmd.m_name);
    }
    m_db.set(cmd.m_args[0], cmd.m_args[1]);
    return "+OK\r\n";
}

std::string CommandHandler::handleGet(const Command& cmd){
    if(cmd.m_args.empty() || cmd.m_args.size() > 1){
        return wrongNumOfArguments(cmd.m_name);
    }
    auto val = m_db.get(cmd.m_args[0]);
    if(val == std::nullopt){
        return "$-1\r\n";
    }
    return "$" + std::to_string(val->size()) + "\r\n" + *val + "\r\n";
}

std::string CommandHandler::processCommand(const Command& cmd){
    auto it = m_handlers.find(cmd.m_name);
    if(it == m_handlers.end()){
        return "-ERR unknown command\r\n";
    }

    return (this->*(it->second))(cmd);

}
