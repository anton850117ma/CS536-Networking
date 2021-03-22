#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define MAXBUFSZM 1000000

void read_bytes(int fd, char* filename);
int send_filename(char* filename, char* ip, char* port);

int send_filename(char* filename, char* ip, char* port){
	
	// Send file name to the server
	int fd, control, bytes;
	struct sockaddr_in addr_ser;
	char buf[2] = "";
	
	// Fill up server's ip address and port number
	bzero(&addr_ser, sizeof(addr_ser));
    addr_ser.sin_family = AF_INET; 
    addr_ser.sin_addr.s_addr = inet_addr(ip);
	addr_ser.sin_port = htons(atoi(port));

	// Connect to the server
	fd = socket(AF_INET, SOCK_STREAM, 0);
	connect(fd, (struct sockaddr *)&addr_ser, sizeof(addr_ser));

	// Send the filename and wait for response
	send(fd, filename, strlen(filename), 0);
	bzero(&buf, sizeof(buf));
	bytes = read(fd, buf, sizeof(buf));

	// Determine what to do next with the response
	// If the response is 0 or 1, return 0 and terminate
	// Otherwise, return the file descriptor
	if(bytes > 0){
		control = buf[0] - '0';
		if(control == 0)printf("The file is not existed.\n");
		else if(control == 1)printf("The file is empty.\n");
		else return fd;
	}
	
	return 0;
}

void read_bytes(int fd, char* filename){

	// Read bytes and calucate the time, speed, and file size.
	struct timeval start, end;
	FILE *file;
	unsigned long filesize = 0;
	char buffer[MAXBUFSZM];
	char username[50], hostname[50], path[150], *pch;
	int bytes;
	double speed, time;

	bzero(&buffer, sizeof(buffer));

	// Create a new file or clear the existing file
	gethostname(hostname, 50);
	getlogin_r(username, 50);
	pch = strtok(hostname,".");
	sprintf(path, "/tmp/%s%s%s", filename, pch, username);
	path[strlen(path)] = '\0';
	file = fopen(path,"w");
	fclose(file);

	// Open the file in appending mode
	file = fopen(path, "a");

	// Read bytes and write to the file
	gettimeofday(&start,NULL);
	while((bytes = read(fd, buffer, MAXBUFSZM)) > 0){
		fwrite(buffer, 1, bytes, file);
		filesize += bytes;
		bzero(&buffer, bytes);
	}
    gettimeofday(&end,NULL);
	fclose(file);

	// Calculate output values
	time = 1000000 * (end.tv_sec-start.tv_sec) + end.tv_usec - start.tv_usec;
	speed = filesize * 8000000 / time;

	// Show the outputs
	printf("Completion time (msec): %.3f\n", time/1000.0);
	printf("Speed (bps): %lf\n", speed);
	printf("File size (bytes): %ld\n", filesize);

}

int main(int argc, char *argv[]) 
{ 
    
	int control;
	
	// Send the filename to server
	control = send_filename(argv[1], argv[2], argv[3]);

	// If receive 2, read bytes.
	if(control != 0)read_bytes(control, argv[1]);

   	return 0; 
} 
