CFLAGS = -Wall -std=c99
LFLAGS = -lm
CC = gcc

main: main.c write_ppm.c decoder.c die.c
	$(CC) $(CFLAGS) main.c write_ppm.c decoder.c die.c $(LFLAGS) -o main
