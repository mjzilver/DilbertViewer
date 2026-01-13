EXEC := DilbertViewer
BUILD_DIR := out
SRC_DIR := src

CPP_FILES := $(shell find $(SRC_DIR) -name "*.cpp")
H_FILES := $(shell find $(SRC_DIR) -name "*.h")

MAKE_FLAGS := -j$(shell nproc --ignore=1)

.PHONY: all build build-debug run debug valgrind clean format release

all: run

build: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) $(MAKE_FLAGS) $(EXEC)

build-debug: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) $(MAKE_FLAGS) $(EXEC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/$(EXEC)

debug: build-debug
	gdb ./$(BUILD_DIR)/$(EXEC)

valgrind: build-debug
	valgrind --leak-check=full ./$(BUILD_DIR)/$(EXEC)

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(CPP_FILES) $(H_FILES)
