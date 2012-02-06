# Fairly lame support for platform-specific configuration

SYSTEM=$(shell uname -s)

ifeq ($(SYSTEM),Darwin)
USE_LIBUSB = 1
LFLAGS += -framework IOKit -framework CoreFoundation
CFLAGS += -I/usr/local/include
LIBUSB := /usr/local/lib/libusb.a
endif
