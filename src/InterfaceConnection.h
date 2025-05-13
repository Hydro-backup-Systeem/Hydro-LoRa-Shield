#ifndef InterfaceConnection_H  // Include guard to prevent multiple inclusions
#define InterfaceConnection_H 

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class InterfaceConnection {
  private:
    int server, client_socket;
    struct sockaddr_in address;
    int address_length = sizeof(address);

    const int port;

  public: 
    // Constructor and deconstructor
    InterfaceConnection(int port);
    ~InterfaceConnection() {};

    void shutdown();

    // Extra connection/handling methods
    bool createSocket();
    void acceptConnection();
    void clientHandling();
    void closeConnection();

};

#endif
