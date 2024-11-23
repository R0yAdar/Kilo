kilo: kilo.c
	$(CC) kilo.c fileio.c editor_operations.c row_operations.c syntax_highlighting.c lifetime.c input.c drawing.c datatypes.c io.c -o kilo -Wall -Wextra -pedantic -std=c99