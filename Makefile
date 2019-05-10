CC=clang
CFLAGS = -std=c99 -Wall -pedantic -g -Llib64 -lruntime
SOURCE=$(wildcard *.c)
TARGET = pa2

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) -o $@ $^ $(CFLAGS)


.PHONY: clean
clean:
	rm *.log $(TARGET)

.PHONY: tar
tar:
	tar zcf $(TARGET).tar.gz ../$(TARGET)/
