CC=x86_64-w64-mingw32-gcc
CFLAGS=-Wall
LDFLAGS=-ladvapi32 -lpsapi
TARGET=watchdog.exe
SRCS=watchdog.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	   $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	   rm -f $(TARGET)
