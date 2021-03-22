#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

int main(int argc, char *argv[]) 
{ 
	struct sockaddr_in addr;
	pid_t k;
	int fd, count, port,addr_len = sizeof(addr);
	char buffer[50], *commands[10], *pch;
	
	// Build up a socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	// Fill up server's address and port number
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET; 
   	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = 0;
	
	// Bind to local host
	bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	
	// Get port number and print
	getsockname(fd, (struct sockaddr *)&addr, &addr_len);
	port = ntohs(addr.sin_port);
	fprintf(stderr, "%d\n",  port);

	while(1){
		
		// Receive commands
		bzero(buffer, sizeof(buffer));
		recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addr_len);
		
		// Parse commends
		memset(commands, '\0', sizeof(char*) * 10);
		count = 0;
		pch = strtok(buffer," ");
      		while(pch != NULL){
          		commands[count] = pch;
          		pch = strtok(NULL," ");
          		count++;
      		}

		// Execute commands
		k = fork();
      		if(k==0){
			if(execvp(commands[0],commands) == -1)exit(1);
      		}
	}
	return 0;
}