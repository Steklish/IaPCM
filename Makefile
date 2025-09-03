CXX = g++
CXXFLAGS = -std=c++17 -Wall -IC:\vcpkg\installed\x64-windows\include
LDFLAGS = -lpthread -lws2_32 -lmswsock
TARGET = skls_server
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
