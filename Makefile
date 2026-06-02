# Waveplay

# Copyright (C) 2025 by Steve Clark

TARGET = waveplay

CC = gcc

CFLAGS =  -g -O -Wall -pedantic -MMD
LDFLAGS = -lSDL2

SOURCE = main.o wave.o

all:	$(SOURCE)
	$(CC) $(SOURCE) -o $(TARGET) $(LDFLAGS)

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SOURCE) $(TARGET)
	rm -f *.d

install:	all
	cp $(TARGET) ~/.local/bin

-include *.d

# Waveplay
