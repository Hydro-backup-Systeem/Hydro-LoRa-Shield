#include "InterfaceConnection.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>    // for timeval
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

#include "../lib/packethandler.h"

extern std::mutex        phMutex;
extern PacketHandler     packetHandler;
extern std::atomic<bool> shutdown_flag;

void InterfaceConnection::dispatch_on_data(void* arg, uint8_t* data, size_t data_len) {
  InterfaceConnection* conn = static_cast<InterfaceConnection*>(arg);
  conn->on_data(arg, data, data_len);
}

InterfaceConnection::InterfaceConnection() {
  unix_socket = std::make_unique<UnixSocket>("/tmp/hydro.sock", 256, dispatch_on_data, this);
  unix_socket->start();
 }

void InterfaceConnection::on_data(void* arg, uint8_t* data, size_t data_len) {
  std::string message((char*)data, data_len);
  std::cout << "Received: " << message << std::endl;

  {
    std::lock_guard<std::mutex> lk(phMutex);
    if (message.find("PRESET:") != std::string::npos) {
      uint8_t presetID = message[7] - '0';
      packetHandler.send((uint8_t*)&presetID, sizeof(presetID), PacketTypes::DICT);
    } else if (message.find("FLAG:") != std::string::npos) {
      std::cout << "Sending flags" << std::endl;
      char flagID = atoi(message.substr(5, 1).c_str());
      packetHandler.send((uint8_t*)&flagID, sizeof(flagID), PacketTypes::FLAGS);
    } else {
      packetHandler.send((uint8_t*)data, data_len, PacketTypes::MSG);
    }
  }
}

void InterfaceConnection::sendToClient(const char* data, size_t length){
  unix_socket->send_data((uint8_t*)data, length);  
}

void InterfaceConnection::shutdown() {
  unix_socket->stop();
}
