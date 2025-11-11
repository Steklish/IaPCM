CXX = g++
CXXFLAGS = -std=c++20 -I"C:/vcpkg/installed/x64-windows/include" -I"C:\msys64\ucrt64\include\opencv4"   -I./labs
LDFLAGS = -L"C:/msys64/ucrt64/lib"

LIBS = -lpthread -lws2_32 -lmswsock -lPowrProf -lsetupapi -lopencv_videoio -lopencv_imgcodecs -lopencv_highgui -lopencv_imgproc -lopencv_core -lCfgmgr32 -static-libgcc -static-libstdc++

TARGET = skls_server
SRC = main.cpp $(wildcard labs/*.cpp)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)