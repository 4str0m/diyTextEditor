CC = gcc
CFLAGS = -D_DEBUG -std=c99 -Wall -Wextra -pedantic

all: kilo

kilo: kilo.c
	$(CC) -D_DEBUG -o kilo kilo.c


clean:
	-rm -rf *.o kilo


