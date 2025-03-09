CXX = g++
# CXXFLAGS = -Wall -Wextra -std=c++17 -O2

SRC = main.cpp src/InterfaceConnection.cpp src/LoraHandling.cpp
OBJ = main.o src/InterfaceConnection.o src/LoraHandling.o
EXEC = lora

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

main.o: main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/InterfaceConnection.o: src/InterfaceConnection.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

src/LoraHandling.o: src/LoraHandling.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean