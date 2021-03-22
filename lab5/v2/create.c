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

#define ANS 3

int length;
struct sockaddr_in addr_create, addr_first;

unsigned char* gen_payload(int number, char *routers[]);

// Generate the payload
unsigned char* gen_payload(int number, char *routers[]){

    int ind_payload = 0, ind_routers = 1, len = number * 4 - 3;
    unsigned short tmp_port;
    unsigned char tmp_ip, mask = 0xff;
    unsigned char* payload = (unsigned char*)malloc(len * sizeof(unsigned char));
    char* token;

    // k
    payload[ind_payload++] = ((number - 3)/2) & mask;

    // Make the payload
    while(ind_routers < number){

        token = strtok(routers[ind_routers++], ".");
        payload[ind_payload++] = '#';

        // Store ip
        while (token != NULL) {
            sscanf(token, "%hhu", &tmp_ip);
            payload[ind_payload++] = tmp_ip;
            token = strtok(NULL, ".");
        }
        payload[ind_payload++] = '#';

        // Store port number
        sscanf(routers[ind_routers++], "%hu", &tmp_port);
        payload[ind_payload++] = tmp_port & mask;
        payload[ind_payload++] = (tmp_port >> 8) & mask;

    }
    length = len;
    return payload;
}

int main(int argc, char *argv[]) {

    struct hostent *host_entry;
    char hostbuffer[256],  *IPbuffer;
    unsigned char payload_backwards[3];
    unsigned char* payload_forwards;
    unsigned short port;
    int fd, bytes, addrlen = sizeof(addr_first);

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
	bzero(&addr_create, sizeof(addr_create));
    addr_create.sin_family = AF_INET;
    addr_create.sin_addr.s_addr = inet_addr(IPbuffer);
    addr_create.sin_port = 0;

    // Bind the socket
    if(bind(fd, (struct sockaddr *)&addr_create, sizeof(addr_create)) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Fill up first router's address
    bzero(&addr_first, sizeof(addr_first));
    addr_first.sin_family = AF_INET;
    addr_first.sin_addr.s_addr = inet_addr(argv[1]);
    addr_first.sin_port = htons(atoi(argv[2]));

    // Generate payload
    payload_forwards = gen_payload(argc, argv);

    // Send the payload to the first router
    while(1){
        bytes = sendto(fd, payload_forwards, length, 0, (struct sockaddr *)&addr_first, sizeof(addr_first));
        if(bytes > 0) break;
    }

    // Receive response from the first router
    while(1){
        bytes = recvfrom(fd, payload_backwards, sizeof(payload_backwards), 0, (struct sockaddr *)&addr_first, &addrlen);
        if(bytes > 0) break;
    }

    // Check if the response is ANS
    if(payload_backwards[0] == ANS){
        port = payload_backwards[1] + (payload_backwards[2] << 8);
        printf("router port: %hu\n", port);
    }
    else printf("The request has been rejected.\n");

    free(payload_forwards);
    return 0;
}