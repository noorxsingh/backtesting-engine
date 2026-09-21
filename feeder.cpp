#include <iostream>
#include <fstream>
#include <string> 
#include <sys/socket.h> // these are the socket functions
#include <netinet/in.h> //address struct and htons
#include <arpa/inet.h> //address conversion helpers
#include <unistd.h> //read, write, close

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        std::cerr << "socket failed\n";
        return 1; 
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        std::cerr << "bind failed\n";
        return 1;
    }

    if (listen(server_fd, 1) == -1) {
        std::cerr << "listen failed\n";
        return 1;
    }

    std::cout << "feeder is listening on port 5000\n";

    sockaddr_in client_addr;
    socklen_t clientlen = sizeof(client_addr); 

    int client_fd = accept(server_fd, (sockaddr*)&client_addr, &clientlen); 
    if (client_fd == -1) {
        std::cerr << "accept failed \n";
        return 1;
    }

    std::ifstream file("data.csv"); 
    std::string line;
    while (std::getline(file, line)) {
        line += "\n";
        write(client_fd, line.c_str(), line.size()); 

    }
    close(client_fd);
    close(server_fd); 
};