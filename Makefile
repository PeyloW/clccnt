CXX      := clang++
CXXFLAGS := -std=c++23 -Wall -Wextra -Wpedantic -O2
LDFLAGS  :=

PREFIX   := /usr/local
BUILD_DIR := build
SRC_DIR   := src

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
BIN  := $(BUILD_DIR)/clccnt

.PHONY: all clean install uninstall verify

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/clccnt

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/clccnt

verify: $(BIN)
	python3 verification/verify.py $(BIN)

clean:
	rm -rf $(BUILD_DIR)
