MVTOOL_DIR		=/usr/local/linaro-aarch64-2020.09-gcc10.2-linux5.4
CROSS_COMPILE	=$(MVTOOL_DIR)/bin/aarch64-linux-gnu-
CC      		= ${CROSS_COMPILE}gcc
CXX    			= ${CROSS_COMPILE}g++
AR      := ${CROSS_COMPILE}ar

CFLAGS = -I../include -O3 -pipe -g0 -feliminate-unused-debug-types -fPIC -mlittle-endian -MMD 
ARFLAGS = rcs

LIBS	= -lpthread -lm
SRC_DIRS := $(shell find . -type d -not -path "./build/*")
BUILD_DIR = build

SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET_LIB = libasplnr.a

# Default target
.PHONY: default
default: $(TARGET_LIB)

# Phony target
.PHONY: all
all: clean $(TARGET_LIB)

$(TARGET_LIB): $(OBJECTS)
		$(AR) $(ARFLAGS) $@ $^ -s
	cp $(TARGET_LIB) ../

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(LIBS) -c $< -o $@

# Include the dependency files
-include $(wildcard *.d)

clean:
	$(RM) -r $(BUILD_DIR) $(TARGET_LIB)
	rm -f ../$(TARGET_LIB)