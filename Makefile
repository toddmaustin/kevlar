CXX      = g++
CXXFLAGS = -std=c++17 -g -Wall -Wextra -O3 -maes -msse4.1 -Wno-unused-but-set-variable -Wno-volatile-register-var -Wno-register -fno-inline -fpermissive -static
TARGET   = test_kevlar
SOURCES  = test_kevlar.cpp

all: build test

announce:
	lsb_release -a
	uname -a
	$(CXX) --version

build: announce $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

test: announce $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o

