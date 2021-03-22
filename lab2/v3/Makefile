all: myftpd myftp

CC = gcc

myftp: client.o
	$(CC) -o $@ client.o
client.o: client.c
	$(CC) -c client.c
myftpd: server.o
	$(CC) -o $@ server.o
server.o: server.c
	$(CC) -c server.c
clean:
	rm *.o myftpd myftp
