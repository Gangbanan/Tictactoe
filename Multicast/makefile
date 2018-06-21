CC = gcc
CFLAGS  = -g -Wall -std=c99

all: tictactoeServer tictactoeClient

tictactoeServer: server.c
	$(CC) $(CFLAGS) -o tictactoeServer server.c

tictactoeClient: client.c
	$(CC) $(CFLAGS) -o tictactoeClient client.c

clean:
	rm tictactoeServer tictactoeClient
