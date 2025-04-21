# Compiler and flags
CXX = g++
CXXFLAGS = -Wall -g

# Source and object files
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Executable name
TARGET = emulator

# Default target
all: $(TARGET)

# Link all object files into the final executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pattern rule: run the program with any filename as target
%.nes: $(TARGET)
	./$(TARGET) $@

# Clean build files
clean:
	rm -f $(TARGET) $(OBJECTS)
