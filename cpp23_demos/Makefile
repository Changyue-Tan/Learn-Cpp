# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -pedantic -O2
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build

# Find all cpp files in src directory
SOURCES = $(wildcard $(SRC_DIR)/*.cpp)
# Generate executable names (replace .cpp with nothing and put in build dir)
TARGETS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%)

# Default target
all: $(TARGETS) | $(BUILD_DIR)

# Build each cpp file into its own executable
$(BUILD_DIR)/%: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Clean build directory
clean:
	rm -rf $(BUILD_DIR)

# Run a specific program (usage: make run FILE=filename without .cpp)
run: $(BUILD_DIR)/$(FILE)
	./$(BUILD_DIR)/$(FILE)

# Run all programs sequentially
run-all: $(TARGETS)
	@for executable in $(TARGETS); do \
		echo "Running $$executable..."; \
		./$$executable; \
		echo "------------------------"; \
	done

# List all available targets
list:
	@echo "Available executables:"
	@for source in $(SOURCES); do \
		program=$$(basename $$source .cpp); \
		echo "  - $$program"; \
	done

# Rebuild everything
rebuild: clean all

# Phony targets
.PHONY: all clean run run-all list rebuild

# Show variables (for debugging)
debug:
	@echo "SOURCES: $(SOURCES)"
	@echo "TARGETS: $(TARGETS)"