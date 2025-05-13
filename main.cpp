#include <wiringPi.h>
#include <iostream>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "./src/InterfaceConnection.h"
#include "./lib/packethandler.h"

// Globals
std::mutex              phMutex;
PacketHandler           packetHandler;
std::atomic<bool>       shutdown_flag{false};

// IRQ signaling
std::mutex              irq_mtx;
std::condition_variable irq_cv;
bool                    irq_happened = false;

void gpioInterrupt()
{
  {
    std::lock_guard<std::mutex> lk(irq_mtx);
    irq_happened = true;
  }
  irq_cv.notify_one();
}

int main()
{
  // SIGINT → set shutdown_flag
  std::signal(SIGINT, [](int){
    shutdown_flag.store(true);
  });

  // wiringPi init + ISR
  if (wiringPiSetup() == -1) {
    std::cerr << "wiringPi setup failed.\n";
    return 1;
  }

  pinMode(7, INPUT);
  
  if (wiringPiISR(7, INT_EDGE_RISING, &gpioInterrupt) < 0) {
    std::cerr << "Failed to setup ISR for GPIO7.\n";
    return 1;
  }

  // PacketHandler init
  {
    std::lock_guard<std::mutex> lk(phMutex);
    packetHandler.init();
    packetHandler.set_long_range();
    packetHandler.set_msg_callback([](uint8_t* data, size_t len){
      std::printf("MSG received: %.*s\n", (int)len, data);
    });
  }

  // Start socket server
  InterfaceConnection server(8080);
  server.createSocket(); // now with accept timeout

  packetHandler.set_long_range();
  packetHandler.receive_mode();

  std::thread socket_thread([&](){
    while (!shutdown_flag.load()) {
      server.acceptConnection();      // will timeout after ~1 s
      if (shutdown_flag.load())      // re-check
        break;

      server.clientHandling();
    }
  });

  // Start LoRa receive thread
  std::thread receive_thread([&](){
    std::unique_lock<std::mutex> lk(irq_mtx);
    while (!shutdown_flag.load()) {
      irq_cv.wait_for(lk, std::chrono::seconds(1), [&](){
        return irq_happened || shutdown_flag.load();
      });

      if (shutdown_flag.load()) break;
      if (!irq_happened) continue;
      irq_happened = false;

      std::lock_guard<std::mutex> phl(phMutex);
      packetHandler.receive();
    }
  });

  std::thread houseKeepingThread([&]() {
    while (!shutdown_flag.load()) {
      if (phMutex.try_lock()) {
        packetHandler.poll();
        packetHandler.clean();
        phMutex.unlock();
      }

      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  // Poll for shutdown
  while (!shutdown_flag.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Clean‐up
  server.shutdown();  
  socket_thread.join();
  receive_thread.join();
  return 0;
}
