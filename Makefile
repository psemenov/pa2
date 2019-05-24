CC=clang
CFLAGS=-g -std=c99 -Wall -pedantic -Llib64 -lruntime

all: 
	$(CC) $(CFLAGS) main.c ipc.c proc.c bank_robbery.c -o pa3

clean:
	rm main
	rm *.log