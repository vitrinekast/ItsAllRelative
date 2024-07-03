# Arduino Library base folder and example structure
EXAMPLES_BASE = sketch
EXAMPLE ?= cli_sketch.ino

# Arduino CLI executable name and directory location
ARDUINO_CLI = arduino-cli

# Arduino CLI Board type
BOARD_TYPE ?= arduino:avr:nano

# Default port to upload to
SERIAL_PORT ?= /dev/cu.usbserial-21230

# Optional verbose compile/upload trigger
V ?= 0
VERBOSE=

# Build path -- used to store built binary and object files
BUILD_DIR=_build
BUILD_PATH=$(PWD)/$(EXAMPLES_BASE)/$(EXAMPLE)/$(BUILD_DIR)

ifneq ($(V), 0)
    VERBOSE=-v
endif

.PHONY: all example program clean

all: example

example:
		$(ARDUINO_CLI) compile $(VERBOSE) -b $(BOARD_TYPE) .

program:
		$(ARDUINO_CLI) compile $(VERBOSE) -b $(BOARD_TYPE) .
		$(ARDUINO_CLI) upload $(VERBOSE) -p $(SERIAL_PORT) --fqbn $(BOARD_TYPE) .

clean:
		@rm -rf $(BUILD_PATH)
		@rm $(EXAMPLES_BASE)/$(EXAMPLE)/*.elf
		@rm $(EXAMPLES_BASE)/$(EXAMPLE)/*.hex