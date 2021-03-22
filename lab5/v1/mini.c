#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <time.h>

#define ANS '3'

struct sockaddr_in addr_mini, addr_super;

unsigned char* gen_payload(char* ip, char* port);

// Generate the payload
unsigned char* gen_payload(char* ip, char* port){

    unsigned short tmp_port;
    unsigned char tmp_ip, mask = 0xff, *payload = (unsigned char*)malloc(6*sizeof(unsigned char));
    char* token = strtok(ip, ".");
    int index = 0;

    // Store server ip into payload
    while (token != NULL) {
        sscanf(token, "%hhu", &tmp_ip);
        payload[index++] = tmp_ip;
        token = strtok(NULL, ".");
    }

    // Store server port into payload
    sscanf(port, "%hu", &tmp_port);
    payload[index++] = tmp_port & mask;
    payload[index++] = (tmp_port >> 8) & mask;

    return payload;
}

int main(int argc, char *argv[]) {

    struct hostent *host_entry;
    char hostbuffer[256],  *IPbuffer;
    unsigned char payload_backwards[3];
    unsigned char* payload_forwards;
    unsigned short port;
    int fd, bytes, addrlen = sizeof(addr_super);

	// Build up the socket
	fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
		perror("opening datagram socket");
		exit(1);
	}

    // Retrieve hostname
    gethostname(hostbuffer, sizeof(hostbuffer));

    // Retrieve host information
    host_entry = gethostbyname(hostbuffer);

    // Convert an Internet network address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));

	// Fill up local address
	bzero(&addr_mini, sizeof(addr_mini));
    addr_mini.sin_family = AF_INET;
    addr_mini.sin_addr.s_addr = inet_addr(IPbuffer);
    addr_mini.sin_port = 0;

    // Bind the socket
    if(bind(fd, (struct sockaddr *)&addr_mini, sizeof(addr_mini)) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Fill up vpn address
    bzero(&addr_super, sizeof(addr_super));
    addr_super.sin_family = AF_INET;
    addr_super.sin_addr.s_addr = inet_addr(argv[1]);
    addr_super.sin_port = htons(atoi(argv[2]));

    // Generate payload
    payload_forwards = gen_payload(argv[3], argv[4]);

    // Send payload to the vpn as a request
    while(1){
        bytes = sendto(fd, payload_forwards, 6, 0, (struct sockaddr *)&addr_super, sizeof(addr_super));
        if(bytes > 0) break;
    }

    // Receive response from the vpn
    while(1){
        bytes = recvfrom(fd, payload_backwards, sizeof(payload_backwards), 0, (struct sockaddr *)&addr_super, &addrlen);
        if(bytes > 0) break;
    }

    // Check if the response is ANS
    if(payload_backwards[0] == ANS){
        port = payload_backwards[1] + (payload_backwards[2] << 8);
        printf("vpn port: %hu\n", port);
    }
    else printf("The request has been rejected.\n");

    return 0;
}