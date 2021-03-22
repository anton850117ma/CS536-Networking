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

void send_command(char* command, char* ip, char* port);

void send_command(char* command, char* ip, char* port){
	
	int fd, result, count = 0, bytes;
	struct sockaddr_in addr_ser;
	struct timeval tv;
  	fd_set readfds;
	char buffer[1024];
	
	// Fill up server's ip address and port number
	bzero(&addr_ser, sizeof(addr_ser));
    	addr_ser.sin_family = AF_INET; 
    	addr_ser.sin_addr.s_addr = inet_addr(ip);
	addr_ser.sin_port = htons(atoi(port));
	
	// Try to send the command at most 3 times
	while(count < 3){
		
		// Update count number, reset fd_set, and build up new socket
		count++;
		FD_ZERO(&readfds);
		fd = socket(AF_INET, SOCK_STREAM, 0);
		
		// Connect to the server and send the command
		connect(fd, (struct sockaddr *)&addr_ser, sizeof(addr_ser));
		send(fd, command, strlen(command), 0);
		
		// Use select() to monitor the socket and wait 2 seconds for server's response 
		FD_SET(fd, &readfds);
		tv.tv_sec = 2;
  		tv.tv_usec = 0;
		result = select(fd+1, &readfds, NULL, NULL, &tv);
		
		// Determine what to do by select()'s return value
		if(result == -1)exit(1);				// an error occurs
		else if(result == 0)close(fd);				// time out, so close the socket
		else{							// something sent by the server
			bzero(&buffer, sizeof(buffer));
			if(read(fd, buffer, 1024) > 0){
				// If number of bytes is greater than 0
				printf("%s\n", buffer);
				close(fd);
				return;
			}
			else{
				// The connection was closed by the server, so close the socket
				close(fd);
				printf("%d try failed!\n", count);
			}
		}
	}
	printf("I give up!\n");
}
 
int main(int argc, char *argv[]) 
{ 
    		
	// Concatenate commands into one
	char command[20] = "";
	int rest = 3, len;
	while(rest < argc){
		strcat(command, argv[rest]);
		strcat(command, " ");
		rest++;
	}
	len = strlen(command);
	command[len-1] = '\0';
	
	// Send the command to server
	send_command(command, argv[1], argv[2]);

   	return 0; 
} 