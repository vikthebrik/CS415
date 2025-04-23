# Makefile for the pseudo-shell executable program
all : pseudo-shell
	

pseudo-shell: main.o string_parser.o command.o
	gcc -g -o pseudo-shell main.o string_parser.o command.o
	
main.o: main.c
	gcc -g -c main.c
	
string_parser.o: string_parser.c string_parser.h
	gcc -g -c string_parser.c
command.o: command.c command.h
	gcc -g -c command.c
	
clean:
	rm -f core *.o pseudo-shell
