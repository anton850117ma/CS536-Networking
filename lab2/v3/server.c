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
#include <sys/time.h>
#include <signal.h>

struct sockaddr_in addr;
struct itimerval new_value;
int times, left, seq, fd, bytes, timeout, num = 3;
char *rebuf;

void signalHandler(int signo);
void sender(int filesize, int blocksize);

void signalHandler(int signo){
	
	// Resend packets
	if(num > 0){

		// If resending the last packet
		if(rebuf[0] == '2'){
			num--;
		}
		while(1){
			bytes = sendto(fd, rebuf, strlen(rebuf), 0, (struct sockaddr *)&addr, sizeof(addr));
			if(bytes > 0)break;
		}
		// Reset alarm
    		new_value.it_value.tv_sec = 0;
    		new_value.it_value.tv_usec = timeout;
		setitimer(ITIMER_REAL, &new_value, NULL);
	}
	else exit(1);
}

void sender(int filesize, int blocksize){
	
	int addr_len = sizeof(addr);
	times = filesize/blocksize;
	left = filesize%blocksize;
	char buffer[blocksize+1], last[left+1], ACK[1] = "", base = '0';

	
	// Set packet values and initialize sequence number
	memset(buffer, '3', sizeof(buffer));
	memset(last, '3', sizeof(last));
	rebuf = buffer;
	seq = 0;
	last[0] = '2';
	
	// Set handler and clean alarm
	signal(SIGALRM, signalHandler);
	memset(&new_value, 0, sizeof(new_value));
	
	while(seq < times){
		
		// Send packets
		buffer[0] = base + seq%2;
		bytes = sendto(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, sizeof(addr));

		if(bytes > 0){

			// Start alarm
			new_value.it_value.tv_sec = 0;
    		new_value.it_value.tv_usec = timeout;
   			setitimer(ITIMER_REAL, &new_value, NULL);

			// Receive ACK
			bytes = recvfrom(fd, ACK, sizeof(ACK), 0, (struct sockaddr *)&addr, &addr_len);
			
			// Stop alarm
			new_value.it_value.tv_sec = 0;
    		new_value.it_value.tv_usec = 0;
    		setitimer(ITIMER_REAL, &new_value, NULL);
			seq++;
		}
	}
	
	// Send the last packet
	rebuf = last;
	while(1){
		bytes = sendto(fd, last, sizeof(last), 0, (struct sockaddr *)&addr, sizeof(addr));
		if(bytes > 0)break;
	}
	
	// Set alarm and resend only three times (by handler)
	new_value.it_value.tv_sec = 0;
    	new_value.it_value.tv_usec = timeout;
   	setitimer(ITIMER_REAL, &new_value, NULL);
	
	bytes = recvfrom(fd, ACK, sizeof(ACK), 0, (struct sockaddr *)&addr, &addr_len);
	
	// Show results
	if(num == 0)printf("Failed to send the last packet.\n");
	else printf("Completed!\n");
}

int main(int argc, char *argv[]) 
{ 

	char *buf[5], *pch, input[100];
	int count = 0, filesize, blocksize, port;
	
	// Get values from argv[]
	filesize = atoi(argv[1]);
	blocksize = atoi(argv[2]);
	timeout = atoi(argv[3]);
	port = atoi(argv[5]);	


	// Build socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);	
	
	// Fill up client's address and port number
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET; 
   	addr.sin_addr.s_addr = inet_addr(argv[4]);
	addr.sin_port = htons(port);
	
	// Begin to send packets
	sender(filesize, blocksize);
	
	return 0;
}