#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main() {

    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
}