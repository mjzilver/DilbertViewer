SRC_DIR := src
VENV_DIR := venv
PYTHON := python3
PIP := $(VENV_DIR)/bin/pip
PY := $(VENV_DIR)/bin/python

.PHONY: install run format clean

all: install run

$(VENV_DIR):
	$(PYTHON) -m venv $(VENV_DIR) && $(PIP) install -U pip

install: $(VENV_DIR)
	$(PIP) install -e ".[dev]"

run: install
	$(PY) -m downloader

format: $(VENV_DIR)
	$(VENV_DIR)/bin/black . --exclude "$(VENV_DIR)"

clean:
	rm -rf $(VENV_DIR)