#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "./src/InterfaceConnection.h"
#include "./lib/packethandler.h"
#include "./lib/lora.h"

// Globals
LoRa                    lora;
PacketHandler           packetHandler(&lora);

std::mutex              phMutex;
std::atomic<bool>       shutdown_flag{false};

// IRQ signaling
std::mutex              irq_mtx;
std::condition_variable irq_cv;
std::atomic<bool>       irq_happened{false};

// Start socket server
InterfaceConnection server;

int main()
{
  // SIGINT → set shutdown_flag
  std::signal(SIGINT, [](int){
    shutdown_flag.store(true);
  });

  lora.init();
  lora.set_long_range();

  // PacketHandler init
  {
    std::lock_guard<std::mutex> lk(phMutex);
    packetHandler.init();
    packetHandler.set_msg_callback([](uint8_t* data, size_t len){
      std::printf("MSG received: %.*s\n", (int)len, data);
      // Forward message to Flutter client
      server.sendToClient(reinterpret_cast<const char*>(data), len);
    });
  }

  std::thread houseKeepingThread([&]() {
    while (!shutdown_flag.load()) {
      packetHandler.poll();
      packetHandler.clean();
    }
  });

  // Poll for shutdown
  while (!shutdown_flag.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Clean‐up
  server.shutdown();  
  houseKeepingThread.join();

  return 0;
}
