all: myfetchfiled myfetchfile

CC = gcc

myfetchfile: client.o
	$(CC) -o $@ client.o
client.o: client.c
	$(CC) -c client.c
myfetchfiled: server.o
	$(CC) -o $@ server.o
server.o: server.c
	$(CC) -c server.c
clean:
	rm *.o myfetchfiled myfetchfile
