all:streamerd playaudio
streamerd: streamerd.o 
	gcc -o streamerd streamerd.o
streamerd.o: streamerd.c debug.h
	gcc -c streamerd.c debug.h
playaudio: playaudio.c debug.h
	gcc -o playaudio playaudio.c debug.h -lasound
clean:
	rm -f *.o streamerd playaudio *.gch

