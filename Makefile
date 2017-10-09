
CC = gcc
CFLAGS = -std=c99 -Wall -


all: textEditor page line

textEditor: main.o page.o line.o
	$(CC) -o textEditor main.o page.o line.o

main.o: main.c page.h
	$(CC) -o main.o -c main.c $(CFLAGS) 

page.o: page.c page.h
	$(CC) -o page.o -c page.c $(CFLAGS) 

line.o: line.c line.h
	$(CC) -o line.o -c line.c $(CFLAGS) 

clean:
	-rm -rf *.o textEditor


