#pragma once
#include <atomic>
#include "../include/CommandHandler.h"

class Server{
    public:
        Server(CommandHandler& ch, int port = 6379); //check if ch can be const &
        ~Server();
        void run();
        void shutdown();

    private:
        static void handleSIGINT(int signum);
        void setupSignalHandler();
        static Server* m_instance;

        int m_port;
        int m_sockfd;
        std::atomic<bool> m_running;
        CommandHandler& m_ch;

        //need to delete copy and assignment, since we keep sockfd, we dont want a copy of it somewhere
        //else where it can be closed twice
};