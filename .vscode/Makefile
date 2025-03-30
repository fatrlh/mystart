CC=x86_64-w64-mingw32-gcc
CFLAGS=-Wall -D_UNICODE -DUNICODE
LDFLAGS=-lgdi32 -luser32
TARGET=watchdog.exe
SRCS=watchdog.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	   $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	   rm -f $(TARGET)
