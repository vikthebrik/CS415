# Makefile for Duck Park Project
CC = gcc
CFLAGS = -g -pthread -lpthread
TARGET = park

all: $(TARGET)

$(TARGET): park.c
	$(CC) $(CFLAGS) -o $(TARGET) park.c

clean:
	rm -f $(TARGET)
