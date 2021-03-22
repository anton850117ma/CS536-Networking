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
#include <time.h>

void client_thread(int fd);

void client_thread(int fd){
	
	struct sockaddr_in address;
    	int coin, count, addrlen = sizeof(address);
	char buffer[20], cli_addr[20], *buf[10], *pch;
	
	// Set random coin value and read the command to buffer 
	coin = rand()%10;
	bzero(&buffer, sizeof(buffer));
	read(fd, buffer, 20);
	
	if(coin < 5){
	
		// If head, ignore the command with showing a message and close the socket
		getpeername(fd, (struct sockaddr *)&address, &addrlen);
		strncpy(cli_addr, inet_ntoa(address.sin_addr), 20);
		dup2(fileno(stdout), 1);
		printf("The request [%s] from [%s] has been ignored!\n", buffer, cli_addr);
		shutdown(fd, SHUT_RDWR);
		close(fd);
    	}
	else{

		// If tail, parse the buffer and execute the command
		count = 0;
		pch = strtok(buffer," ");
        	while(pch != NULL){
            		buf[count] = pch;
            		pch = strtok(NULL," ");
            		count++;
        	}
		dup2(fd, fileno(stdout));
		close(fd);
		if(execvp(buf[0],buf) == -1)exit(1);
	}
}

int main(int argc, char const *argv[]) 
{ 
    	struct sockaddr_in address;
	int fd, new_fd, port, addrlen = sizeof(address);;  
    	char input[50];
	pid_t k;
    
 	srand(time(NULL));
	
	// Build up the socket
    	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) 
    	{ 
        	perror("socket failed"); 
        	exit(EXIT_FAILURE); 
    	} 
    
	// Get IP address
	//scanf("%50[^\n]", input);
	
	// Fill up address structure
	bzero(&address, sizeof(address));
    	address.sin_family = AF_INET; 
    	address.sin_addr.s_addr = inet_addr(argv[1]); 
    	address.sin_port = 0; 
    
	// Bind the socket
    	if (bind(fd, (struct sockaddr *)&address, sizeof(address))<0) 
    	{ 
        	perror("bind failed"); 
        	exit(EXIT_FAILURE);
    	}
	
	// Get assigned port number
	getsockname(fd, (struct sockaddr *)&address, &addrlen);
	port = ntohs(address.sin_port);
	fprintf(stderr, "%d\n",  port);
	
	// Listen
    	if (listen(fd, 5) < 0) 
    	{ 
        	perror("listen"); 
        	exit(EXIT_FAILURE); 
    	}
	
	// Keep listening to connection request
	// If connect successfully, fork a child process to deal with client's message
	while(1){
		new_fd = accept(fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
		if(new_fd < 0) continue;
		
		k = fork();
		if(k == 0) client_thread(new_fd);
	}	
	return 0; 
} 