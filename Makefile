CXX = g++
CXXFLAGS = -Wall -std=c++20 -O2

SRC = main.cpp src/InterfaceConnection.cpp src/LoraHandling.cpp lib/lora_sx1276.cpp lib/packethandler.cpp
OBJ = main.o src/InterfaceConnection.o src/LoraHandling.o lib/lora_sx1276.o lib/packethandler.o
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

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean