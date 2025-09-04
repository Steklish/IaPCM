CXX = g++
CXXFLAGS = -std=c++20 -Wall -IC:\vcpkg\installed\x64-windows\include -I./labs
LDFLAGS = -lpthread -lws2_32 -lmswsock -lPowrProf
TARGET = skls_server
SRC = main.cpp $(wildcard labs/*.cpp)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
