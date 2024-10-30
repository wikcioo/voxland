ifndef config
    config=debug
endif

CXXFLAGS := -Wall -Wextra -Werror -Wshadow -Wswitch-enum -Wconversion -pedantic \
			-fstack-protector -fsanitize=undefined \
			-std=c++20

ifeq ($(config), debug)
    CXXFLAGS += -DDEBUG -g -O0
else ifeq ($(config), release)
    CXXFLAGS += -DNDEBUG -DENABLE_ASSERTIONS=0 -O3
endif

BUILD_DIR := build/$(config)

CLIENT_LIBS    := $(shell pkg-config --libs glfw3 glew freetype2)
CLIENT_INCS    := -Iclient/lib -I. -Ithird_party $(shell pkg-config --cflags freetype2)
CLIENT_SOURCES := $(wildcard client/*.cpp)
CLIENT_OBJECTS := $(addprefix $(BUILD_DIR)/client/, $(addsuffix .cpp.o, $(basename $(notdir $(CLIENT_SOURCES)))))

CLIENT_SO_INCS    := -I. -Ithird_party
CLIENT_SO_SOURCES := $(wildcard client/lib/*.cpp)
CLIENT_SO_OBJECTS := $(addprefix $(BUILD_DIR)/client/lib/, $(addprefix lib, $(addsuffix .so, $(basename $(notdir $(CLIENT_SO_SOURCES))))))

SERVER_LIBS    := $(shell pkg-config --libs sqlite3)
SERVER_INCS    := -I. -Ithird_party
SERVER_SOURCES := $(wildcard server/*.cpp)
SERVER_OBJECTS := $(addprefix $(BUILD_DIR)/server/, $(addsuffix .cpp.o, $(basename $(notdir $(SERVER_SOURCES)))))

COMMON_INCS    := -Ithird_party -I.
COMMON_SOURCES := $(wildcard common/*.cpp)
COMMON_SOURCES += $(wildcard common/memory/*.cpp)
COMMON_SOURCES += $(wildcard common/collections/*.cpp)
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
$(BUILD_DIR)/client/client: $(CLIENT_OBJECTS) $(COMMON_OBJECTS)
	$(CXX) $^ $(CXXFLAGS) $(CLIENT_LIBS) -Wl,-rpath,$(BUILD_DIR)/client/lib -o $@

$(BUILD_DIR)/client/%.cpp.o: client/%.cpp
	$(CXX) -c $< $(CLIENT_INCS) $(CXXFLAGS) -fPIC -o $@

$(BUILD_DIR)/client/lib/lib%.so: client/lib/%.cpp $(COMMON_OBJECTS) $(BUILD_DIR)/client/renderer2d.cpp.o $(BUILD_DIR)/client/shader.cpp.o $(BUILD_DIR)/client/texture.cpp.o
	$(CXX) $(CLIENT_SO_INCS) $(CXXFLAGS) -shared -fPIC $^ -o $@

# Server targets
$(BUILD_DIR)/server/server: $(SERVER_OBJECTS) $(COMMON_OBJECTS)
	$(CXX) $^ $(CXXFLAGS) $(SERVER_LIBS) -o $@

$(BUILD_DIR)/server/%.cpp.o: server/%.cpp
	$(CXX) -c $< $(SERVER_INCS) $(CXXFLAGS) -DSERVER -o $@

# Common targets
$(BUILD_DIR)/common/%.cpp.o: common/%.cpp
	$(CXX) -c -fPIC $< $(COMMON_INCS) $(CXXFLAGS) -o $@

$(BUILD_DIR)/common/%.cpp.o: common/memory/%.cpp
	$(CXX) -c -fPIC $< $(COMMON_INCS) $(CXXFLAGS) -o $@

$(BUILD_DIR)/common/%.cpp.o: common/collections/%.cpp
	$(CXX) -c -fPIC $< $(COMMON_INCS) $(CXXFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)
