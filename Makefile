#

CC=c++
CFLAGS=-std=gnu++11 -Wall -Wextra -Werror -pedantic

all: main

main: main.cpp
	$(CC) $(CFLAGS) main.cpp -o webclient

clean:
	rm -f webclient *.o
#
