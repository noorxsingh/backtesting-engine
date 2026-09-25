#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue> 
#include <string>
#include <sstream>
#include <sys/socket.h> // these are the socket functions
#include <netinet/in.h> //address struct and htons
#include <arpa/inet.h> //address conversion helpers
#include <unistd.h> //read, write, close
#include "backtester.hpp" //for the actual connection to bars


std::queue<Bar> q;
std::condition_variable cv;
std::mutex m;
bool done = false;


void producer() {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        std::cerr << "failed to set up kernal communication \n";
        return; 
    }
    
    sockaddr_in rAddr;
    rAddr.sin_family = AF_INET;
    rAddr.sin_port = htons(5000);
    rAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock_fd, (sockaddr*)&rAddr, sizeof(rAddr)) == -1) {
        std::cerr << "connection failed or refused\n";
        return;
    }
    std::cout << "connected to feeder \n";

    std::string stash; 
    char buf[4096];
    uint64_t index = 0;
    while (true) {
        ssize_t n = read(sock_fd, buf, sizeof(buf));
        if (n <= 0) {
            std::cout << "broke out of read loop \n";
            break;
        }
        stash.append(buf, n); 
        size_t pos = stash.find("\n");

        while ((pos = stash.find('\n')) != std::string::npos) {
            std::string line = stash.substr(0, pos);
            stash.erase(0, pos + 1); 

            try {
                std::string field;
                std::stringstream ss(line);
        

                std::getline(ss, field, ','); 

                std::getline(ss, field, ',');
                double open = std::stod(field);

                std::getline(ss, field, ',');
                double high = std::stod(field);

                std::getline(ss, field, ',');
                double low = std::stod(field);

                std::getline(ss, field, ',');
                double close = std::stod(field);

                std::getline(ss, field, ',');
                double volume = std::stod(field);

                Bar b;
                b.timestamp = index;
                b.open = open;
                b.high = high;
                b.low = low;
                b.close = close;
                b.volume = volume;
                {
                std::lock_guard<std::mutex> lock(m);
                q.push(b);
                }
                cv.notify_one();
                index++;
            } catch (const std::exception& e) {
                std::cerr << "Skipped" << e.what() << "\n";
                continue; 
            }
        }
    }
    {
        std::lock_guard<std::mutex> lock(m);
        done = true;
        cv.notify_one();
    }
    close(sock_fd);
};

void consumer() {
    std::vector<Bar> bars;

    while (true) {
        std::unique_lock<std::mutex> lock(m);
        cv.wait(lock, []{ return !q.empty() || done; });
        if (q.empty() && done) break;

        Bar b = q.front();
        q.pop();
        lock.unlock();

        bars.push_back(b);                    
    }
    SmaCrossover sma(20, 50);  
    FlatCost cost(.001); 
    BacktestingEngine engine(bars, &sma, 10000.0, &cost, .5);
    engine.run();
    std::cout << "total return: " << engine.totalReturn() << std::endl;
};

int main() {
    std::thread t1(producer);
    std::thread t2(consumer);
    t1.join();
    t2.join(); 
};