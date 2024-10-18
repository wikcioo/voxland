BUILD_DIR  := build
TESTS_DIR  := src
COMMON_DIR := ../common

TEST_SOURCES := $(wildcard $(TESTS_DIR)/memory/*.cpp)
TEST_SOURCES += $(wildcard $(TESTS_DIR)/collections/*.cpp)
TEST_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .cpp.o, $(basename $(notdir $(TEST_SOURCES)))))

COMMON_SOURCES := $(COMMON_DIR)/log.cpp
COMMON_SOURCES += $(wildcard $(COMMON_DIR)/memory/*.cpp)
COMMON_SOURCES += $(wildcard $(COMMON_DIR)/collections/*.cpp)
COMMON_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .cpp.o, $(basename $(notdir $(COMMON_SOURCES)))))

MANAGER_SOURCES := $(wildcard *.cpp)
MANAGER_OBJECTS := $(addprefix $(BUILD_DIR)/, $(addsuffix .cpp.o, $(basename $(notdir $(MANAGER_SOURCES)))))

CXXFLAGS := -Wall -Wextra -Werror -Wshadow -Wswitch-enum -Wconversion -pedantic \
			-fstack-protector -fsanitize=undefined \
			-DDEBUG -DENABLE_ASSERTIONS -g \
			-I../common -I. \
			-DENABLE_LOGGING=0 \
			-std=c++20

all:
	@echo "Building test suite..."
	@mkdir -p $(BUILD_DIR)
	@make --no-print-directory $(BUILD_DIR)/test_suite

$(BUILD_DIR)/test_suite: $(TEST_OBJECTS) $(MANAGER_OBJECTS) $(COMMON_OBJECTS)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/%.cpp.o: $(TESTS_DIR)/memory/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.cpp.o: $(TESTS_DIR)/collections/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.cpp.o: ./%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.cpp.o: $(COMMON_DIR)/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.cpp.o: $(COMMON_DIR)/memory/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/%.cpp.o: $(COMMON_DIR)/collections/%.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)
