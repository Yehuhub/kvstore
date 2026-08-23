#pragma once
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "../include/CommandHandler.h"

struct Worker{
    std::thread m_thread;
    int m_fd;
};

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

        CommandHandler& m_ch;
        int m_port;
        int m_sockfd;
        std::atomic<bool> m_running;

        std::unordered_map<std::thread::id, Worker> m_threads;
        std::vector<std::thread::id> m_finishedThreads;
        std::mutex m_mutex;

        void handleClient(int clientFd);
        void reapFinishedThreads();

        //need to delete copy and assignment, since we keep sockfd, we dont want a copy of it somewhere
        //else where it can be closed twice
};