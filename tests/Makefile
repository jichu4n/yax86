# filepath: /home/chuan/Projects/yax86/tests/Makefile
# Makefile for yax86 test suite

# Compiler and flags
CXX = c++
CXXFLAGS = -std=c++14 -Wall -Wextra -g
LDFLAGS = -lgtest -lgtest_main -pthread

# Parent library
PARENT_DIR = ..
LIB_NAME = libyax86.a
LIB_PATH = $(PARENT_DIR)/$(LIB_NAME)

# Find all test files and generate target names
TEST_SRCS = $(wildcard *_test.cpp)
TEST_BINS = $(TEST_SRCS:.cpp=)
TEST_RUNS = $(TEST_SRCS:.cpp=.run)

# Default target: build all tests
all: $(TEST_BINS)

# Linking rule for test binaries
%_test: %_test.cpp $(LIB_PATH)
	$(CXX) $(CXXFLAGS) -I$(PARENT_DIR) $< -o $@ $(LIB_PATH) $(LDFLAGS)

# Run a single test
%.run: %
	@mkdir -p ./test_output
	./$<

# Run all tests
run: $(TEST_RUNS)
	@echo "All tests completed successfully"

# Clean generated files
clean:
	rm -rf $(TEST_BINS) ./test_output/

# Build parent library if needed
$(LIB_PATH):
	$(MAKE) -C $(PARENT_DIR)

.PHONY: all run clean