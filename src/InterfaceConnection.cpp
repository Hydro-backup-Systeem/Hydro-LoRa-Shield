#include "InterfaceConnection.h"
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <iostream>

InterfaceConnection::InterfaceConnection(int port) : port(port), server(0), client_socket(0), address_length(sizeof(address)) {}

InterfaceConnection::~InterfaceConnection() { // Destructor to close the connection if needed
  closeConnection();
}

bool InterfaceConnection::createSocket() {  // Create socket file descriptor
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  // Create the socket
  server = socket(AF_INET, SOCK_STREAM, 0);
  if (server == 0) {
      perror("Socket failed");
      return false;
  }

  // Bind socket (give correct address)
  if (bind(server, (struct sockaddr*)&address, sizeof(address)) < 0) {
      perror("Bind failed");
      return false;
  }

  // Listen for connections
  if (listen(server, 3) < 0) {
      perror("Listen failed");
      return false;
  }

  return true; // Success
}

void InterfaceConnection::createConnection(){
  std::cout << "Waiting for a connection..." << std::endl;

  while (true) {
      client_socket = accept(server, (struct sockaddr*)&address, (socklen_t*)&address_length);
      if (client_socket < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // Timeout occurred, retry
              std::cout << "Accept timed out, retrying..." << std::endl;
              std::this_thread::sleep_for(std::chrono::seconds(1));
              continue;
          } else {
              // Other error
              perror("Accept failed");
              return;
          }
      }

      std::cout << "Client connected!" << std::endl;
      break; // Exit the loop if a connection is successful
  }
}

void InterfaceConnection::clientHandling(){
    char buffer[1024] = {0};
    while (true) {
        int valread = read(client_socket, buffer, 1024);
        if (valread <= 0) {
            std::cout << "Client disconnected or error occurred." << std::endl;
            break; // Exit the loop if the client disconnects
        }

        std::string received(buffer, valread);
        std::cout << "Received: " << received << std::endl;

        // Echo back the message
        std::string response = "C++ Server: " + received;
        send(client_socket, response.c_str(), response.size(), 0);
    }

    close(client_socket); // Close the client socket after disconnect
    client_socket = 0;    // Reset the client socket
}

void InterfaceConnection::closeConnection(){
  if (client_socket > 0) close(client_socket);
  if (server > 0) close(server);
  std::cout << "Server closed." << std::endl;
}