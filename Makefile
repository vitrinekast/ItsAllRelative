# Arduino Library base folder and example structure
EXAMPLES_BASE = sketch
FILE ?= 20240610_ArduinoEurorackSequencer.ino

# Arduino CLI Board type
BOARD_TYPE ?= arduino:avr:nano

# Default port to upload to
SERIAL_PORT ?= /dev/cu.usbserial-110

# Optional verbose compile/upload trigger
V ?= 0
VERBOSE=

# Build path -- used to store built binary and object files
BUILD_DIR=_build
BUILD_PATH=$(PWD)/$(EXAMPLES_BASE)/$(EXAMPLE)/$(BUILD_DIR)

ifneq ($(V), 0)
    VERBOSE=-v
endif

compile:
	clear
	arduino-cli compile -p ${SERIAL_PORT} -b ${BOARD_TYPE} ${FILE}

upload: compile
	arduino-cli upload -p ${SERIAL_PORT} --fqbn ${BOARD_TYPE} ${FILE}

dev: compile upload
	arduino-cli monitor -p $(SERIAL_PORT) -b $(BOARD_TYPE) --timestamp


# arduino-cli upload -p /dev/ttyACM0 --fqbn arduino:samd:mkr1000 MyFirstSketch