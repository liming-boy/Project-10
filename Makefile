# Makefile for Word Search Generator
# Run "make" to build, "make run" to build+run, "make clean" to tidy up.

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2
TARGET  = wordsearch
SOURCES = main.c common.c grid.c fileio.c game.c ui.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = common.h grid.h fileio.h game.h ui.h

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS) $(TARGET) $(TARGET).exe

.PHONY: all run clean
