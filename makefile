CXX := g++
BUILD_DIR := build
BIN_DIR := bin
TARGET := Server

SRCS := Event/Eventcplus.cpp FtpServer/FtpServer.cpp Server.cpp

DEBUG_FLAGS := -g3 -O0 -D_DEBUG
RELEASE_FLAGS := -O3

CXXFLAGS := -std=c++20 -Wall -Wextra
LDFLAGS := -levent -levent_pthreads -lpthread

DEBUG_OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/debug/%.o)
RELEASE_OBJS := $(SRCS:%.cpp=$(BUILD_DIR)/release/%.o)

all: release

debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: $(BIN_DIR)/debug/$(TARGET)

release: CXXFLAGS += $(RELEASE_FLAGS)
release: $(BIN_DIR)/release/$(TARGET)

$(BIN_DIR)/debug/$(TARGET): $(DEBUG_OBJS)
	@mkdir -p $(@D)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BIN_DIR)/release/$(TARGET): $(RELEASE_OBJS)
	@mkdir -p $(@D)
	$(CXX) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/debug/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/release/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all debug release clean