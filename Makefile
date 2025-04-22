# Compiler
CXX = g++
CXXFLAGS = -Wall -g `sdl2-config --cflags`

# Source and object files
SOURCES = $(wildcard *.cpp)
OBJECTS = $(SOURCES:.cpp=.o)

# Executable name
TARGET = emulator

# SDL linker flags
LDFLAGS = `sdl2-config --libs`

# Default target
all: $(TARGET)

# Link all object files into the final executable
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pattern rule: run the program with any filename as target
%.nes: $(TARGET)
	@echo "Running with file: $@"
	./$(TARGET) $@

# Clean build files
clean:
	rm -f $(TARGET) $(OBJECTS)
