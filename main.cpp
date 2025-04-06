#include <wiringPi.h>
#include <iostream>
#include <atomic>
#include <cstdio>
#include <cstring>

#include <thread>
#include <chrono>
#include <csignal>

// Include your PacketHandler and other Lora handling code
#include "./lib/packethandler.h"

// This is what you receive from Packettype::ENCRY, it is the counter of AES CTR
uint32_t iv[] = { 0xcf099dad, 0xcc6321be, 0xd2236383, 0x2ddb2c4d };

// This is an encrypted message that could be received from Packettype::MSG
uint32_t cipher[] = { 0x297e4e7e, 0xf2d423aa, 0x48254554, 0x78700746, 0xda212de4, 0x71c88d49, 0x8ff51ec8, 0xbbf8b182 };

// Define the state machine as before
enum class State {
  Init,
  Send,
  Receive,
  Wait,
  Shutdown
};

// Use an atomic state variable to share state between ISR and main thread.
static std::atomic<State> state{State::Init};
static std::atomic<bool>  shutdown_flag(false);

// PacketHandler instance (global for simplicity)
static PacketHandler packetHandler;

// GPIO ISR callback for GPIO7 (wiringPi numbering)
// This function will be called when a falling edge is detected on GPIO7.
void gpioInterrupt() {
  state.store(State::Receive);
}

int main()
{
  srand(time(NULL));

  // Initialize wiringPi; use wiringPi pin numbering.
  if (wiringPiSetup() == -1) {
    std::cerr << "wiringPi setup failed." << std::endl;
    return 1;
  }

  // Set up GPIO7 as input for the interrupt (using wiringPi numbering)
  pinMode(7, INPUT);

  // Register the ISR for GPIO7: trigger on falling edge.
  if (wiringPiISR(7, INT_EDGE_RISING, &gpioInterrupt) < 0) {
    std::cerr << "Failed to setup ISR for GPIO7." << std::endl;
    return 1;
  }

  signal(SIGINT, [](int signum) {
    state.store(State::Shutdown);
    shutdown_flag.store(true);
  });

  std::thread t1([]() {
    while (true) {
      state.store(State::Send);
      std::this_thread::sleep_for(std::chrono::seconds(1));

      if (shutdown_flag.load()) break;
    }
  });

  int i = 0;
  char buffer[512];

  packetHandler.set_long_range();

  // Main loop: state machine similar to your STM code.
  while (true) {
    switch (state.load()) {

      case State::Init:
        // Transition to wait state after initialization.
        state.store(State::Wait);
        // Set packetHandler in receive mode.
        packetHandler.receive_mode();
        break;

      case State::Send:
        // You can add Send state handling here.
        // Send iv key
        packetHandler.send(reinterpret_cast<uint8_t*>(iv), sizeof(iv) * sizeof(uint32_t), PacketTypes::ENCRYP);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        packetHandler.send(reinterpret_cast<uint8_t*>(cipher), sizeof(cipher) * sizeof(uint32_t), PacketTypes::MSG);

        // Back to receive mode
        packetHandler.receive_mode();
        break;

      case State::Receive:
        // Process incoming packet.
        packetHandler.receive();

        // we simply return to Wait state.
        state.store(State::Wait);
        break;

      case State::Wait:
        // Clean up expired messages or do other housekeeping.
        packetHandler.clean();
        break;

      case State::Shutdown:
        t1.join();
        return 0;        

      default:
        std::cerr << "Unexpected state encountered!" << std::endl;
        break;
    }
  }

  return 0;
}
