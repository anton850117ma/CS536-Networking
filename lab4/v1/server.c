#include <unistd.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include "linchar.h"

int main(int argc, char *argv[]) 
{ 
	FILE *file;
	struct sockaddr_in addr;
	pid_t k;
	int fd, result, port, bytes, addr_len = sizeof(addr);
	char req_buffer[50], file_buffer[50], **commands, **ip_pubkey, *client_ip, *pubkey;
	
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
		bzero(req_buffer, sizeof(req_buffer));
		recvfrom(fd, req_buffer, sizeof(req_buffer), 0, (struct sockaddr *)&addr, &addr_len);

		// Get Client ip address and open acl.txt
		client_ip = inet_ntoa(addr.sin_addr);
		file = fopen("acl.txt", "r");
		result = 1;
		
		// Compare ip addresses to check whether the client is valid
		bzero(&file_buffer, sizeof(file_buffer));
		while((bytes = fread(file_buffer, 1, sizeof(file_buffer), file))>0) {	
			ip_pubkey = parse(file_buffer);
			result = compare(ip_pubkey[0], client_ip);
			if(result == 0) break;	// 0 = matched, 1 = not found
			bzero(&file_buffer, bytes);
		}
		fclose(file);
		
		// The client's ip address is not stored in the file
		if(result == 1){
			printf("Client ip address is not found!\n");
			continue;
		}

		// Check if commands exceed 30 bytes. If so, drop it.
		if(strlen(req_buffer) > 30){
			printf("The command exceeds 30 bytes!\n");
			continue;
		}

		// Decode the command
		pubkey = ip_pubkey[1];
		for(int i = 0; i < strlen(req_buffer); ++i){
			req_buffer[i] = mydecode(req_buffer[i], pubkey, i%4);
		}

		// Parse the commend
		commands = parse(req_buffer);

		// Execute the command
		k = fork();
    	if(k==0){
			if(execvp(commands[0],commands) == -1)exit(1);
			free_char(commands);
			free_char(ip_pubkey);
    	}
		
		// Prevent from creating zombies
		signal(SIGCHLD,SIG_IGN);
	}
	return 0;
}