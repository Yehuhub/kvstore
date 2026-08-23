#include "../include/Server.h"
#include "../include/Database.h"
#include "../include/CommandHandler.h"
#include <string>
#include <iostream>

//for testing
#include <thread>
#include <chrono>

int main(int argc, char * argv[]){
    int port = 6379;
    if (argc > 1){
        port = std::stoi(argv[1]);
    }

    Database db;
    CommandHandler ch = CommandHandler(db);
    Server dbServer(ch, port);
    dbServer.run();

    
}