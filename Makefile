SRC_FILES := $(wildcard ./src/*.c)

kilo: $(SRC_FILES)
	$(CC) $(SRC_FILES) -o kilo -Wall -Wextra -pedantic -std=c99