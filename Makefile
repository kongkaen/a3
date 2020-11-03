CC = gcc

main: test.c
	$(CC) -o smallsh test.c

clean:
	rm -f smallsh
