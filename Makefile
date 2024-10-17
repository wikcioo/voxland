ifndef config
    config=debug
endif

CFLAGS := -Wall -Wextra -Werror -Wshadow -Wswitch-enum -Wconversion -pedantic \
		  -fstack-protector -fsanitize=undefined

ifeq ($(config), debug)
    CFLAGS += -DDEBUG -DENABLE_ASSERTIONS -g -O0
else ifeq ($(config), release)
    CFLAGS += -DNDEBUG -O3
endif

BUILD_DIR := build/$(config)

CLIENT_LIBS    := $(shell pkg-config --libs glfw3 glew)
CLIENT_INCS    := -Iclient/lib -Icommon
CLIENT_SOURCES := $(wildcard client/*.cpp)
CLIENT_OBJECTS := $(addprefix $(BUILD_DIR)/client/, $(addsuffix .cpp.o, $(basename $(notdir $(CLIENT_SOURCES)))))

CLIENT_SO_INCS    := -Icommon
CLIENT_SO_SOURCES := $(wildcard client/lib/*.cpp)
CLIENT_SO_OBJECTS := $(addprefix $(BUILD_DIR)/client/lib/, $(addprefix lib, $(addsuffix .so, $(basename $(notdir $(CLIENT_SO_SOURCES))))))

SERVER_LIBS    := $(shell pkg-config --libs sqlite3)
SERVER_INCS    := -Icommon
SERVER_SOURCES := $(wildcard server/*.cpp)
SERVER_OBJECTS := $(addprefix $(BUILD_DIR)/server/, $(addsuffix .cpp.o, $(basename $(notdir $(SERVER_SOURCES)))))

COMMON_SOURCES := $(wildcard common/*.cpp)
COMMON_OBJECTS := $(addprefix $(BUILD_DIR)/common/, $(addsuffix .cpp.o, $(basename $(notdir $(COMMON_SOURCES)))))

.PHONY: all client server common clean

all:
	@echo "($(config)) Building all..."
	@make --no-print-directory common
	@make --no-print-directory client
	@make --no-print-directory server

client:
	@echo "($(config)) Building client..."
	@mkdir -p $(BUILD_DIR)/client/lib
	@make --no-print-directory $(CLIENT_SO_OBJECTS)
	@make --no-print-directory $(BUILD_DIR)/client/client

server:
	@echo "($(config)) Building server..."
	@mkdir -p $(BUILD_DIR)/server
	@make --no-print-directory $(BUILD_DIR)/server/server

common:
	@echo "($(config)) Building common..."
	@mkdir -p $(BUILD_DIR)/common
	@make --no-print-directory $(COMMON_OBJECTS)

# Client targets
$(BUILD_DIR)/client/client: $(CLIENT_OBJECTS)
	$(CXX) $^ $(CFLAGS) $(CLIENT_LIBS) -o $@

$(BUILD_DIR)/client/%.cpp.o: client/%.cpp
	$(CXX) -c $< $(CLIENT_INCS) $(CFLAGS) -o $@

$(BUILD_DIR)/client/lib/lib%.so: client/lib/%.cpp
	$(CXX) $(CLIENT_SO_INCS) -shared -fPIC $< -o $@

# Server targets
$(BUILD_DIR)/server/server: $(SERVER_OBJECTS) $(COMMON_OBJECTS)
	$(CXX) $^ $(CFLAGS) $(SERVER_LIBS) -o $@

$(BUILD_DIR)/server/%.cpp.o: server/%.cpp
	$(CXX) -c $< $(SERVER_INCS) $(CFLAGS) -o $@

# Common targets
$(BUILD_DIR)/common/%.cpp.o: common/%.cpp
	$(CXX) -c $< $(CFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
