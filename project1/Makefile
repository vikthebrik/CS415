CC=gcc
FLAGS=-g -o
EXECUTABLE=pseudoshell


all: $(EXECUTABLE)

pseudoshell: command.o main.o string_parser.o
	$(CC) ${FLAGS) $@ $^

command.o: command.c command.h
	$(CC) -c $<

%.0: %.c 
	$(CC) -c $^

string_parser.o: string_parser.c string_parser.h
	$(CC) -c $<

clean:
	rm -f *.o $(EXECUTABLE)