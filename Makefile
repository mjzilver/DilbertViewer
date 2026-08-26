EXEC := DilbertViewer
DOWNLOADER := dl_downloader
BUILD_DIR := out
SRC_DIR := src
ASSETS_DIR := assets

CPP_FILES := $(shell find $(SRC_DIR) -name "*.cpp")
H_FILES := $(shell find $(SRC_DIR) -name "*.h")

MAKE_FLAGS := -j$(shell nproc --ignore=1)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share
APPDIR = $(DATADIR)/applications
ICONDIR = $(DATADIR)/icons/hicolor/256x256/apps

DESKTOP_FILE = $(ASSETS_DIR)/$(EXEC).desktop
ICON_FILE = $(ASSETS_DIR)/dilbert.png

.PHONY: all build build-debug run debug valgrind clean format tidy release install uninstall

all: run

build: $(BUILD_DIR)
	$(MAKE) $(MAKE_FLAGS) -C downloader build
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE) $(MAKE_FLAGS) $(EXEC)

build-debug: $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug .. && $(MAKE) $(MAKE_FLAGS) $(EXEC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

run: build
	cp downloader/dist/$(DOWNLOADER) $(BUILD_DIR)/$(DOWNLOADER)
	./$(BUILD_DIR)/$(EXEC)

debug: build-debug
	gdb ./$(BUILD_DIR)/$(EXEC)

valgrind: build-debug
	valgrind --leak-check=full \
		--show-leak-kinds=all \
		--track-origins=yes \
		--verbose \
		--log-file=$(BUILD_DIR)/valgrind.log \
		./$(BUILD_DIR)/$(EXEC)

clean:
	rm -rf $(BUILD_DIR)

format:
	clang-format -i $(CPP_FILES) $(H_FILES)
	$(MAKE) $(MAKE_FLAGS) -C downloader format

tidy: $(BUILD_DIR)/Makefile
	clang-tidy -p $(BUILD_DIR) $(CPP_FILES) $(H_FILES)

install: build
	install -m 755 $(BUILD_DIR)/$(EXEC) $(DESTDIR)$(BINDIR)

	install -d $(DESTDIR)$(APPDIR)
	install -m 644 $(DESKTOP_FILE) $(DESTDIR)$(APPDIR)

	install -d $(DESTDIR)$(ICONDIR)
	install -m 644 $(ICON_FILE) $(DESTDIR)$(ICONDIR)

	cd downloader && $(MAKE) build
	install -m 755 downloader/dist/$(DOWNLOADER) $(DESTDIR)$(BINDIR)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(EXEC)
	rm -f $(DESTDIR)$(APPDIR)/$(EXEC).desktop
	rm -f $(DESTDIR)$(ICONDIR)/dilbert.png
	rm -f $(DESTDIR)$(ICONDIR)/$(DOWNLOADER)