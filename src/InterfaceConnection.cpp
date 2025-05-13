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

InterfaceConnection::InterfaceConnection(int port)
 : port(port), server(0), client_socket(0) {}

bool InterfaceConnection::createSocket() {
  server = socket(AF_INET, SOCK_STREAM, 0);
  if (server < 0) {
    perror("Socket failed");
    return false;
  }

  sockaddr_in addr{};
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port        = htons(port);

  if (bind(server, (sockaddr*)&addr, sizeof(addr)) < 0) {
    perror("Bind failed");
    close(server);
    return false;
  }

  if (listen(server, 3) < 0) {
    perror("Listen failed");
    close(server);
    return false;
  }

  return true;
}

void InterfaceConnection::acceptConnection() {
  client_socket = accept(server, nullptr, nullptr);
  if (client_socket < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      // timeout: no client this cycle
      return;
    }
    perror("Accept failed");
    return;
  }
  std::cout << "Client connected!\n";
}

void InterfaceConnection::clientHandling() {
  if (client_socket <= 0) return;

  char buffer[1024];
  while (!shutdown_flag.load()) {
    int valread = read(client_socket, buffer, sizeof(buffer));
    if (valread <= 0) break;  // disconnect or error

    {
      std::cout << "Sending" << std::endl;
      std::lock_guard<std::mutex> lk(phMutex);
      packetHandler.send((uint8_t*)buffer, valread, PacketTypes::MSG);
      packetHandler.receive_mode();
    }

    // echo
    std::string resp = "C++ Server: ";
    resp.append(buffer, valread);
    send(client_socket, resp.data(), resp.size(), 0);
  }

  close(client_socket);
  client_socket = 0;
  std::cout << "Client disconnected.\n";
}

void InterfaceConnection::shutdown() {
  if (server > 0) close(server);
  if (client_socket > 0) close(client_socket);
}
