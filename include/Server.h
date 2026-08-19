#pragma once
#include <atomic>

class Server{
    public:
        Server(int port = 6379);
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

        //need to delete copy and assignment, since we keep sockfd, we dont want a copy of it somewhere
        //else where it can be closed twice
};