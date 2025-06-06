CC = gcc
CXX = g++
CXXFLAGS = -Wall -std=c++20 -O2

SRC = main.cpp src/InterfaceConnection.cpp src/LoraHandling.cpp lib/lora_sx1276.cpp lib/packethandler.cpp lib/aes.c src/unix-socket.cpp lib/lora.cpp
OBJ = main.o src/InterfaceConnection.o src/LoraHandling.o lib/lora_sx1276.o lib/packethandler.o lib/aes.o src/unix-socket.o lib/lora.o
EXEC = lora

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lspidev-lib++ -lwiringPi

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/InterfaceConnection.o: src/InterfaceConnection.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/LoraHandling.o: src/LoraHandling.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lib/lora_sx1276.o: lib/lora_sx1276.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lib/packethandler.o: lib/packethandler.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lib/aes.o: lib/aes.c
	$(CC) -Wall -O2 -c $< -o $@

src/unix-socket.o: src/unix-socket.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

lib/lora.o: lib/lora.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean