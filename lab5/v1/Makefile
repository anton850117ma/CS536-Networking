all: terve minigopher supergopher

CC = gcc

terve: chat.o
	$(CC) -o $@ chat.o
chat.o: chat.c
	$(CC) -c chat.c
minigopher: mini.o
	$(CC) -o $@ mini.o
mini.o: mini.c
	$(CC) -c mini.c
supergopher: super.o
	$(CC) -o $@ super.o
super.o: super.c
	$(CC) -c super.c supergopher.h

clean:
	rm *.o *.gch terve minigopher supergopher
