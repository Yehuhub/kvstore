#include "../include/Server.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdexcept>
#include <unistd.h>
#include <iostream>
#include <string>
#include <signal.h>
#include "../include/RespParser.h"

Server* Server::m_instance = nullptr;

void Server::handleSIGINT(int signum){
    if(m_instance){
        m_instance->m_running = false;
    }
}

void Server::setupSignalHandler(){
    struct sigaction sa{};
    sa.sa_handler = handleSIGINT;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

Server::Server(CommandHandler& ch, int port): m_ch(ch), m_port(port), m_sockfd(-1), m_running(true){
    m_instance = this;
    setupSignalHandler();

    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (m_sockfd < 0) {
        throw std::runtime_error("socket() failed");
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(m_port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_sockfd, (sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        throw std::runtime_error("bind() failed");
    }

    if (listen(m_sockfd, SOMAXCONN) < 0){
        throw std::runtime_error("listen() failed");
    }
}

Server::~Server(){
    if (m_sockfd >= 0){
        close(m_sockfd);
    }
}

void Server::run(){
    std::cout<<"Server running on port " << m_port << std::endl;

    while (m_running){
        int clientFd = accept(m_sockfd, nullptr, nullptr);
        if (clientFd < 0){
            if(m_running){
                std::cerr << "Error accepting client connection!" << std::endl;
            }
            break;
        }


        RespParser parser;
        char buffer[1024];

        while(true){
            ssize_t bytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
            if(bytes == 0) break; //no more messages
            if(bytes < 0){
                if (errno == EINTR) continue;
                std::cerr << "recv() failed" << std::endl;
                break;
            }
            parser.feed(buffer, bytes);
            
            try{
                // try and parse the command we received in the socket
                while(auto cmd = parser.tryParseCommand()){
                    auto response = m_ch.processCommand(*cmd);
                    
                    send(clientFd, response.c_str(), response.size(), 0);
                }
            }catch(const std::exception& e){
                std::string err = std::string("-ERR ") + e.what() + "\r\n";
                send(clientFd, err.c_str(), err.size(), 0);
                break;
            }
            
            
        }

        close(clientFd);
    }

}