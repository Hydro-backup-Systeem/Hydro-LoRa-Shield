#include <iostream>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include "./src/InterfaceConnection.h"

int main() {

    InterfaceConnection server(8080);

    if (!server.createSocket()) return 1;

    server.createConnection();
    server.clientHandling();

    return 0;
}