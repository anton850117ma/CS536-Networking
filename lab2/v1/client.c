#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>

int main(int argc, char *argv[]) 
{ 
	
	
	struct sockaddr_in addr, addr_ser;
	char command[20] = "";
	int fd, rest = 3, len;

	// Concatenate commands into one
	while(rest < argc){
		strcat(command, argv[rest]);
		strcat(command, " ");
		rest++;
	}
	len = strlen(command);
	command[len-1] = '\0';
		
	// Build socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	
	
	// Fill up client's address and port number(useless)
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET; 
   	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = 0;
	
	// Bind to local host(useless)
	bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	
	// Fill up server's address and port number
	bzero(&addr_ser, sizeof(addr_ser));
	addr_ser.sin_family = AF_INET; 
   	addr_ser.sin_addr.s_addr = inet_addr(argv[1]);
	addr_ser.sin_port = htons(atoi(argv[2]));
	
	// Send the command to the server
	sendto(fd, command, sizeof(command), 0, (struct sockaddr *)&addr_ser, sizeof(addr_ser));

	return 0;
}