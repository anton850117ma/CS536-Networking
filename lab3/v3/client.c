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

void receiver(int fd, int when, struct sockaddr_in addr, int addr_len);

void receiver(int fd, int when, struct sockaddr_in addr, int addr_len){
	
	struct timeval tvalBefore, tvalAfter;
	int bytes, port, ending = 0, ack_last = 0,count = 0, seq = 0, total_bytes,  dup_bytes, time;
	double bps;
	char buffer[100000], ACK[1], ser_addr[20], base = '0', exp_seq;
	
	
	while(1){
		
		// Receive packets
		exp_seq = base + seq%2;
		bzero(buffer, sizeof(buffer));
		bytes = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&addr, &addr_len);
		
		// If the first packet, start timer
		if(seq == 0)gettimeofday (&tvalBefore, NULL);
		
		if(buffer[0] == exp_seq){
			// If the packet sequence number is correct
			total_bytes += --bytes;
			seq++;
		}
		else if(buffer[0] == '2'){
			
			// If the last packet
			if(ending == 0)gettimeofday (&tvalAfter, NULL);	//End the time when first time receiving the last packet 
			total_bytes += --bytes;
			ending = 1;
			
		}
		else{
			// If the packet is duplicated
			dup_bytes += --bytes;
		}
		
		// Count number of receiving packets
		count++;
		if(when == -1 || (when > 0 && when != count)){

			// Send ACK if dropwhen equals -1 or counter does not equal dropwhen
			ACK[0] = buffer[0];
			while(1){
				bytes = sendto(fd, ACK, sizeof(ACK), 0, (struct sockaddr *)&addr, sizeof(addr));
				if(bytes > 0)break;
			}
			if(ending)ack_last = 1; //Send last bytes
		}
		else count = 0; // Drop ACK and reset counter
		if(ack_last)break;
	}
	// Print out results
	time = (tvalAfter.tv_sec - tvalBefore.tv_sec)*1000000 + tvalAfter.tv_usec - tvalBefore.tv_usec;
	bps = (double)total_bytes * 8000000 / time; 
	printf("Total bytes: %d\nDuplicated bytes: %d\nTime: %lf\nbps: %lf\n", total_bytes, dup_bytes, time/1000.0, bps);

}

int main(int argc, char *argv[]) 
{ 
	struct sockaddr_in addr;
	int fd, count = 0, dropwhen, port,addr_len = sizeof(addr);	
	
	dropwhen = atoi(argv[2]);
	
	// Build up a socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);

	// Fill up client's address and port number
	bzero(&addr, sizeof(addr));
	addr.sin_family = AF_INET; 
   	addr.sin_addr.s_addr = inet_addr(argv[1]);
	addr.sin_port = 0;
	
	// Bind to local host
	bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	
	// Get port number and print
	getsockname(fd, (struct sockaddr *)&addr, &addr_len);
	port = ntohs(addr.sin_port);
	printf("%d\n", port);
	
	// Begin to receive packets
	receiver(fd, dropwhen, addr, addr_len);

	return 0;
}