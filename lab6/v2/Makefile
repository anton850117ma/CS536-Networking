all: streamerd playaudio

CC = gcc

playaudio: client.o
	$(CC) -o $@ client.o
client.o: client.c
	$(CC) -c client.c
streamerd: server.o
	$(CC) -o $@ server.o
server.o: server.c
	$(CC) -c server.c
clean:
	rm *.o streamerd playaudio
