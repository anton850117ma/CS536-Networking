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
#include <netdb.h> 
#include <time.h>

#define MAXBUFSZM 1000000

int check_state(FILE **file);
void client_thread(int blocksize, int fd);
void get_filename(int blocksize, int port);

int check_state(FILE **file){

	// Check if the file exist
	// If exist, then check if the file is empty
	// 0 >> not existed, 1 >> empty, 2 >> otherwise
	if(*file == NULL) return 0;
	else{
		fseek(*file, 0, SEEK_END);
    	if (ftell(*file) == 0) return 1;
	}
	return 2;
}

void client_thread(int blocksize, int fd){
	
	// If the 
	FILE *file;
	struct sockaddr_in address;
    int bytes, control, addrlen = sizeof(address), total = 0;
	char path[150], filename[100], buf[2] = "", file_buffer[MAXBUFSZM];

	// Wait for the filename
	bzero(&filename, sizeof(filename));
	bytes = read(fd, filename, 150);
	
	if(bytes < 1) return;

	// Open the file and check its state
	sprintf(path, "/tmp/%s", filename);
	file = fopen(path,"r");
	control = check_state(&file);
	
	// Determine what to send
	if(control == 0){			// not existed
		buf[0] = '0';
		send(fd, buf, strlen(buf), 0);

	}
	else if(control == 1){ 		// empty
		buf[0] = '1';
		send(fd, buf, strlen(buf), 0);
	}
	else{						// contain some contents

		buf[0] = '2';
		send(fd, buf, strlen(buf), 0);
		// Close the file to reset reading pointer
		fclose(file);

		// Choose the min between blocksize and MAXBUFSZM
		blocksize = blocksize < MAXBUFSZM ? blocksize : MAXBUFSZM;

		// Open file to read
		file = fopen(path, "r");

		// Read contents in the file and send to the client
		bzero(&file_buffer, sizeof(file_buffer));
		while((bytes = fread(file_buffer, 1, blocksize, file))>0) {
			total += bytes;
    		write(fd, file_buffer, bytes);
			bzero(&file_buffer, bytes);
		}
		fclose(file);
	}

	// Shutdown and close
	shutdown(fd, SHUT_WR);
	if(read(fd, file_buffer, MAXBUFSZM) == 0) close(fd);
	
}

void get_filename(int blocksize, int port){

	// Build up the socket and listen connection and request
	// When connected, fork a child process to get filename and continue further tasks
	struct sockaddr_in address;
	struct hostent *host_entry;
	int fd, new_fd, port2, addrlen = sizeof(address);;  
    char hostbuffer[256], *IPbuffer;
	pid_t k;

	// Build up the socket
	fd = socket(AF_INET, SOCK_STREAM, 0);

    // Retrieve hostname 
    gethostname(hostbuffer, sizeof(hostbuffer)); 
  
    // Retrieve host information 
    host_entry = gethostbyname(hostbuffer);
  
    // Convert an Internet network address into ASCII string 
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])); 

	// Fill up address structure
	bzero(&address, sizeof(address));
    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = inet_addr(IPbuffer); 
    address.sin_port = htons(port);

	// Bind the socket
	port2 = bind(fd, (struct sockaddr *)&address, sizeof(address));
	if(port2 < 0){
		printf("The port number is not available, please try again!\n");
		return;
	}

	// Listen
	listen(fd, 5);

	// Keep listening to connection request
	// If connect successfully, fork a child process to deal with client's message
	while(1){
		new_fd = accept(fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
		if(new_fd < 0) continue;
		
		k = fork();
		if(k == 0) client_thread(blocksize, new_fd);
	}	
}

int main(int argc, char *argv[])
{ 
    
	int blocksize = atoi(argv[1]);
	int port = atoi(argv[2]);

	// Start the process
    get_filename(blocksize, port);

	return 0; 
} 
