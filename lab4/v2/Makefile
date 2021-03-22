all: terve

CC = gcc

terve: chat.o
	$(CC) -o $@ chat.o
chat.o: chat.c
	$(CC) -c chat.c
clean:
	rm *.o terve
