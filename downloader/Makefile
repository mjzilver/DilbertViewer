SRC_DIR := src
VENV_DIR := venv
PYTHON := python3
PIP := $(VENV_DIR)/bin/pip
PY := $(VENV_DIR)/bin/python

.PHONY: setup run format clean build distclean

all: run

$(VENV_DIR):
	$(PYTHON) -m venv $(VENV_DIR)
	$(PIP) install -U pip

setup: $(VENV_DIR)
	$(PIP) install -e ".[dev]"

run: setup
	$(PY) -m downloader

format: $(VENV_DIR)
	$(VENV_DIR)/bin/black . --exclude "$(VENV_DIR)"

build: setup
	$(PY) -m PyInstaller \
		--onefile \
		--name dl_downloader \
		$(SRC_DIR)/downloader/__main__.py

clean:
	rm -rf $(VENV_DIR)

distclean: clean
	rm -rf dist build *.spec