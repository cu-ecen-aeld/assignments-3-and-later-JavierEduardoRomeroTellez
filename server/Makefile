CC = $(CROSS_COMPILE)gcc

default:	server

all:	server

server:	aesdsocket.o
	$(CC) aesdsocket.o -o aesdsocket

aesdsocket.o: aesdsocket.c
	$(CC) -c aesdsocket.c -o aesdsocket.o

clean:
	rm -f aesdsocket.o aesdsocket
