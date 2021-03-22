all: remote-command-srv remote-command-cli

CC = gcc

remote-command-cli: client.o
	$(CC) -o $@ client.o
client.o: client.c
	$(CC) -c client.c
remote-command-srv: server.o
	$(CC) -o $@ server.o
server.o: server.c
	$(CC) -c server.c
clean:
	rm *.o remote-command-srv remote-command-cli
