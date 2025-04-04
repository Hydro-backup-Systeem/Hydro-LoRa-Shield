#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <math.h>

#include "./src/InterfaceConnection.h"
#include "./src/LoraHandling.h"

#include "./lib/lora_sx1276.h"
#include "./lib/packethandler.h"
#include <signal.h>
#include <string.h>

#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <csignal>

std::atomic<bool> shutdown_flag(false);

// PacketHandler instance and mutex
std::mutex lora_mtx;
static PacketHandler packetHandler;

void send_thread() {
    // -- Socket handling 
    // InterfaceConnection server(8080);
    
    // if (!server.createSocket()) return 1;
    
    // server.createConnection();

    char buffer[] = "Testing Lora\n";

    while (true) {
        lora_mtx.lock();
        packetHandler.send((uint8_t *) buffer, sizeof(buffer));
        lora_mtx.unlock();

        std::this_thread::sleep_for(std::chrono::seconds(2));

        // Shutdown
        if (shutdown_flag.load()) break;
    }
}

void receive_thread() {
    while (true) {
        // Continous receive
        lora_mtx.lock();
        packetHandler.poll();
        lora_mtx.unlock();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Shutdown
        if (shutdown_flag.load()) break;
    }
}

int main() {
    std::thread t1(send_thread);
    std::thread t2(receive_thread);

    signal(SIGINT, [](int signum) {
        shutdown_flag.store(true);
    });

    t1.join();
    t2.join();

    return 0;
}