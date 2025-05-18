#ifndef InterfaceConnection_H  // Include guard to prevent multiple inclusions
#define InterfaceConnection_H 

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <memory>

#include "./unix-socket.hpp"

class InterfaceConnection {
  public: 
    // Constructor and deconstructor
    InterfaceConnection();
    ~InterfaceConnection() {};

    void shutdown();
    void sendToClient(const char* data, size_t length);
  
    private:
      // Unix socket
      std::unique_ptr<UnixSocket> unix_socket;
      void on_data(void* arg, uint8_t* data, size_t data_len);

      static void dispatch_on_data(void* arg, uint8_t* data, size_t data_len);
};

#endif
