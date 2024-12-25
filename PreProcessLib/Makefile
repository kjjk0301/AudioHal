# Default target
.PHONY: default
default: $(TARGET)

# Phony target
.PHONY: all
all: clean $(default)

default:
	$(MAKE) -C lib
	$(MAKE) -C exec

clean:
	$(MAKE) -C lib clean
	$(MAKE) -C exec clean