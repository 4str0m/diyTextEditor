
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic


all: textEditor

textEditor: main.o page.o line.o editor.o
	$(CC) -o textEditor main.o page.o line.o editor.o

main.o: main.c page.h
	$(CC) -o main.o -c main.c $(CFLAGS) 

editor.o: editor.c editor.h page.o
	$(CC) -o editor.o -c editor.c $(CFLAGS) 
	

page.o: page.c page.h line.o
	$(CC) -o page.o -c page.c $(CFLAGS) 

line.o: line.c line.h
	$(CC) -o line.o -c line.c $(CFLAGS) 

clean:
	-rm -rf *.o textEditor


