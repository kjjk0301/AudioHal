hide := @
ECHO := echo

MVTOOL_DIR		=/usr/local/linaro-aarch64-2020.09-gcc10.2-linux5.4
CROSS_COMPILE	=$(MVTOOL_DIR)/bin/aarch64-linux-gnu-
CC      		= ${CROSS_COMPILE}gcc
CXX    			= ${CROSS_COMPILE}g++
CFLAGS = -I../include -I/home/docker/Dev/bsp/rootfilesystem/nfsroot/cv2x/3rdparty_cv22/include -O3 -pipe -g0 -feliminate-unused-debug-types -fPIC -mlittle-endian
LIBS	= -lpthread -lm -L/home/docker/Dev/bsp/rootfilesystem/nfsroot/cv2x/cv2x/usr/lib/ -lasound
LDFLAGS = -L../lib -lasplnr
TARGET = AsplNRTest_1mic
OBJS_BIN = main.o keyboard.o 

# Default target
.PHONY: default
default: $(TARGET)

# Phony target
.PHONY: all
all: clean $(TARGET)

$(TARGET): $(OBJS_BIN)
	$(hide)$(ECHO) $(TARGET)
	$(CC) -o $@ $(OBJS_BIN) $(CFLAGS) $(LDFLAGS) $(LIBS)
	cp $(TARGET) ../

main.o: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $<

keyboard.o: keyboard.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) -c -o $@ $<

clean:
	rm $(TARGET) ../$(TARGET) *.o