all: terve createoverlay overlaygopher

CC = gcc

terve: chat.o
	$(CC) -o $@ chat.o
chat.o: chat.c
	$(CC) -c chat.c
createoverlay: create.o
	$(CC) -o $@ create.o
create.o: create.c
	$(CC) -c create.c
overlaygopher: overlay.o
	$(CC) -o $@ overlay.o
overlay.o: overlay.c
	$(CC) -c overlay.c overlaygopher.h

clean:
	rm *.o *.gch terve createoverlay overlaygopher
