EXEC := DilbertViewer
BUILD_DIR := out
SRC_DIR := src

CPP_FILES := $(shell find $(SRC_DIR) -name "*.cpp")
H_FILES := $(shell find $(SRC_DIR) -name "*.h")

.PHONY: all build build-debug run debug clean format release

all: run

build: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) $(EXEC)

build-debug: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) $(EXEC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/$(EXEC)

debug: build-debug
	gdb ./$(BUILD_DIR)/$(EXEC)

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(CPP_FILES) $(H_FILES)
