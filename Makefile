ifndef config
    config=debug
endif

CFLAGS := -Wall -Wextra -Werror -Wshadow -Wswitch-enum -Wconversion -pedantic \
		  -fstack-protector -fsanitize=address,undefined

ifeq ($(config), debug)
    CFLAGS += -DDEBUG -DENABLE_ASSERTIONS -g -O0
else ifeq ($(config), release)
    CFLAGS += -DNDEBUG -O3
endif

BUILD_DIR := build/$(config)

CLIENT_LIBS    := $(shell pkg-config --libs glfw3)
CLIENT_SOURCES := $(wildcard client/*.cpp)
CLIENT_OBJECTS := $(addprefix $(BUILD_DIR)/client/, $(addsuffix .cpp.o, $(basename $(notdir $(CLIENT_SOURCES)))))

SERVER_LIBS    :=
SERVER_SOURCES := $(wildcard server/*.cpp)
SERVER_OBJECTS := $(addprefix $(BUILD_DIR)/server/, $(addsuffix .cpp.o, $(basename $(notdir $(SERVER_SOURCES)))))

.PHONY: all client server clean

all:
	@echo "($(config)) Building all..."
	@make --no-print-directory client
	@make --no-print-directory server

client:
	@echo "($(config)) Building client..."
	@mkdir -p $(BUILD_DIR)/client
	@make --no-print-directory $(BUILD_DIR)/client/client

server:
	@echo "($(config)) Building server..."
	@mkdir -p $(BUILD_DIR)/server
	@make --no-print-directory $(BUILD_DIR)/server/server

# Client targets
$(BUILD_DIR)/client/client: $(CLIENT_OBJECTS)
	$(CXX) $^ $(CFLAGS) $(CLIENT_LIBS) -o $@

$(BUILD_DIR)/client/%.cpp.o: client/%.cpp
	$(CXX) -c $^ $(CFLAGS) -o $@

# Server targets
$(BUILD_DIR)/server/server: $(SERVER_OBJECTS)
	$(CXX) $^ $(CFLAGS) $(SERVER_LIBS) -o $@

$(BUILD_DIR)/server/%.cpp.o: server/%.cpp
	$(CXX) -c $^ $(CFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
