EXEC := DilbertViewer
BUILD_DIR := out
SRC_DIR := src

CPP_FILES := $(shell find $(SRC_DIR) -name "*.cpp")
H_FILES := $(shell find $(INC_DIR) -name "*.h")

all: run

.PHONY: build run clean format

build: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && $(MAKE) $(MAKE_FLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/$(EXEC)

debug: build
	gdb ./$(BUILD_DIR)/$(EXEC)

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(CPP_FILES) $(H_FILES)
