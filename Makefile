CC = gcc

main: smallsh.c
	$(CC) -o smallsh smallsh.c

clean:
	rm -f smallsh
