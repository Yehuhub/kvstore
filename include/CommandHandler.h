#pragma once

#include "../include/Database.h"
#include "../include/Command.h"
#include <string>
#include <vector>


class CommandHandler{
    public:
        CommandHandler(Database& db);
        std::string processCommand(const Command& cmd);

    private:
        using Handler = std::string (CommandHandler::*)(const Command&);

        Database& m_db;
        std::unordered_map<std::string, Handler> m_handlers;

        // common commands
        std::string handlePing(const Command& cmd);
        std::string handleEcho(const Command& cmd);
        std::string handleFlushAll(const Command& cmd);

        // kv operations
        std::string handleSet(const Command& cmd);
        std::string handleGet(const Command& cmd);
        std::string handleKeys(const Command& cmd);
        std::string handleType(const Command& cmd);
        std::string handleDel(const Command& cmd);
        std::string handleRename(const Command& cmd);
        std::string handleExpire(const Command& cmd);

        // list operations
        std::string handlePush(const Command& cmd);
        std::string handlePop(const Command& cmd);
        std::string handleLlen(const Command& cmd);
        std::string handleLindex(const Command& cmd);
        std::string handleLset(const Command& cmd);
        std::string handleLrem(const Command& cmd);
        std::string handleLrange(const Command& cmd);
};