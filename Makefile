-include config.mak

VERSION := 0.5.0

CFLAGS += -std=gnu99 -Wall -D_FILE_OFFSET_BITS=64 -O3 -g -fexpensive-optimizations -fomit-frame-pointer -frename-registers -I/usr/src/linux/include
LFLAGS += -g
LDLIBS += -L. -ltopfield

OBJS=crc16.o daemon.o mjd.o tf_bytes.o tf_io.o tf_fwio.o tf_open.o tf_util.o

ifdef USE_LIBUSB
endif
ifdef USE_LIBUSB
CFLAGS += -DUSE_LIBUSB
LIBUSB ?= -lusb
LDLIBS += $(LIBUSB)
OBJS += usbutil.o 
else
OBJS += usb_io.o usb_io_util.o
endif

all: libtopfield.a test_makename test_swab test_crc

libtopfield.a: $(OBJS)
	$(RM) $@
	$(AR) cvq $@ $(OBJS)

test_makename: test_makename.o tf_util.o libtopfield.a 
	$(CC) $(LFLAGS) -o $@ test_makename.o tf_util.o $(LDLIBS)

test_swab: test_swab.o libtopfield.a 
	$(CC) $(LFLAGS) -o $@ test_swab.o $(LDLIBS)

test_crc: test_crc.o libtopfield.a 
	$(CC) $(LFLAGS) -o $@ test_crc.o $(LDLIBS)

test:
	./test_makename
	./test_swab

clean:
	$(RM) *.o lib*.a test_makename test_swab test_crc core core.* tags

install:
# DO NOT DELETE
