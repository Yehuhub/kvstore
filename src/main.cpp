#include "../include/Server.h"
#include <string>

int main(int argc, char * argv[]){
    int port = 6379;
    if (argc > 1){
        port = std::stoi(argv[1]);
    }

    Server dbServer(port);
    dbServer.run();
}