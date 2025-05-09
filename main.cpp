#include <wiringPi.h>
#include <iostream>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <stdlib.h>

#include <thread>
#include <chrono>
#include <csignal>
#include <time.h>

#include "./src/InterfaceConnection.h"

#include "./lib/aes.hpp"

// Include your PacketHandler and other Lora handling code
#include "./lib/packethandler.h"

// Has to be randomly generated
// This is what you receive from Packettype::ENCRY, it is the counter of AES CTR
uint32_t iv[] = { 0xcf099dad, 0xcc6321be, 0xd2236383, 0x2ddb2c4d };

// Sample encrypted data
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

// Function to reverse endianness in-place per 4-byte block
void fix_endianness_4byte_blocks(uint8_t* buffer, int length) {
  for (int i = 0; i < length; i += 4) {
      uint8_t tmp = buffer[i];
      buffer[i] = buffer[i + 3];
      buffer[i + 3] = tmp;

      tmp = buffer[i + 1];
      buffer[i + 1] = buffer[i + 2];
      buffer[i + 2] = tmp;
  }
}

// Converts an array of 4 uint32_t to 16 bytes
void u32_to_bytes(uint32_t* input, uint8_t* output) {
  for (int i = 0; i < 4; i++) {
      output[i * 4 + 0] = (input[i] >> 24) & 0xFF;
      output[i * 4 + 1] = (input[i] >> 16) & 0xFF;
      output[i * 4 + 2] = (input[i] >> 8) & 0xFF;
      output[i * 4 + 3] = input[i] & 0xFF;
  }
}

uint32_t aes_key[4] = SUPER_SECRET_SANTA_AES_KEY_DO_NOT_SHARE;

int main()
{  
  // Socket with flutter
  // if (wiringPiSetup() == -1) return 1;

  // // Create socket connection
  // InterfaceConnection server(8080);

  // if (!server.createSocket()) {
  //     std::cerr << "Failed to create socket" << std::endl;
  //     return 1;
  // }

  // // Run the connection and client handling in a separate thread
  // std::thread serverThread([&server]() {
  //     server.createConnection();
  //     server.clientHandling();
  // });

  // // Main thread can handle other tasks (e.g., GPIO interrupts or state machine)
  // while (!shutdown_flag.load()) {
  //     // Perform other tasks here
  //     std::this_thread::sleep_for(std::chrono::seconds(1));
  // }

  // // Wait for the server thread to finish before exiting
  // serverThread.join();

  // struct AES_ctx ctx;
  // uint8_t key_bytes[16];
  // uint8_t iv_bytes[16];

  // u32_to_bytes(aes_key, key_bytes);
  // u32_to_bytes(iv, iv_bytes);

  // // Message to encrypt
  // char message[] = "im a cat meow";  // 13 bytes
  // uint8_t plaintext[32] = {0};  // Pad to 32 bytes
  // memcpy(plaintext, message, strlen(message));

  // // Make a working buffer for encryption
  // uint8_t ciphertext[32];
  // memcpy(ciphertext, plaintext, 32);

  // // Encrypt
  // AES_init_ctx_iv(&ctx, key_bytes, iv_bytes);
  // AES_CTR_xcrypt_buffer(&ctx, ciphertext, 32);

  // printf("Encrypted ciphertext:\n");
  // for (int i = 0; i < 32; i++) {
  //     printf("%02x ", ciphertext[i]);
  //     if ((i + 1) % 16 == 0) printf("\n");
  // }

  // // Now decrypt
  // uint8_t decrypted[32];
  // memcpy(decrypted, ciphertext, 32);

  // AES_init_ctx_iv(&ctx, key_bytes, iv_bytes);
  // AES_CTR_xcrypt_buffer(&ctx, decrypted, 32);

  // // Fix endianness after decryption
  // // fix_endianness_4byte_blocks(decrypted, 32);

  // printf("\nDecrypted hex bytes:\n");
  // for (int i = 0; i < 32; i++) {
  //     printf("%02x ", decrypted[i]);
  //     if ((i + 1) % 16 == 0) printf("\n");
  // }

  // printf("\nDecrypted as text: ");
  // for (int i = 0; i < 32; i++) {
  //     if (decrypted[i] >= 32 && decrypted[i] <= 126) {
  //         printf("%c", decrypted[i]);
  //     }
  // }
  // printf("\n");

  // return 0;


  // srand(time(NULL));

  // Initialize wiringPi; use wiringPi pin numbering.
  if (wiringPiSetup() == -1) {
    std::cerr << "wiringPi setup failed." << std::endl;
    return 1;
  }

  // size_t bytes = sizeof(cipher) * sizeof(uint32_t);

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
      std::this_thread::sleep_for(std::chrono::seconds(3));

      if (shutdown_flag.load()) break;
    }
  });

  // int i = 0;
  // char buffer[512];

  packetHandler.init();
  packetHandler.set_long_range();

  packetHandler.set_msg_callback([](uint8_t* data, size_t len) {
    printf("MSG received: %s\n\r", data);
  });

  // Main loop: state machine similar to your STM code.
  while (true) {
    switch (state.load()) {

      case State::Init: 
      {
        // Transition to wait state after initialization.
        state.store(State::Wait);

        // Set packetHandler in receive mode.
        packetHandler.receive_mode();
        break;
      }

      case State::Send:
        // You can add Send state handling here.
        // Send iv key
        {
          const char* message = "i am a cat meow";
          size_t message_len = strlen(message);
  
          packetHandler.send((uint8_t*)message, message_len, PacketTypes::MSG);
          // Back to receive mode
          packetHandler.receive_mode();

          state.store(State::Wait);
          break;
        }

      case State::Receive: {
        // printf("Receive\n\r");
        // Process incoming packet.
        packetHandler.receive();

        // we simply return to Wait state.
        state.store(State::Wait);
        break;
      }

      case State::Wait:
      {
        // Clean up expired messages or do other housekeeping.
        packetHandler.poll();
        packetHandler.clean();
        break;
      }

      case State::Shutdown:
      {
        t1.join();
        return 0;        
      }

      default:
      {
        std::cerr << "Unexpected state encountered!" << std::endl;
        break;
      }
    }
  }

  return 0;
}
