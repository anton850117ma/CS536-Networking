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
#include <sys/time.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#include "overlaygopher.h"

#define MAXSOCKIND 10

// Table structure
typedef struct forwarding_table{
    unsigned short pre_port, post_port;
    unsigned int sd_index, pre_ip, post_ip;
    struct forwarding_table *next;
}table;

table *head = NULL;
struct sockaddr_in addr_super, addr_request, addr_temp, addr_client[MAXSOCKIND*2];
int client_socket[MAXSOCKIND*2], length, isset[MAXSOCKIND*2] = {0};

unsigned short bind_socket(int index);
void forward(int sd, int index);
void insert(unsigned char *request, int index, int master_socket);
void tunnel(int master_socket);
void respond(int socket, unsigned short port);
void free_table(table* current);
void show_table(int index, int show);
void intHandler(int dummy);

// Handle ctrl-c to free the table and close sockets
void intHandler(int dummy){

    free_table(head);
    for(int index = 0; index < MAXSOCKIND*2; ++index) close(client_socket[index]);
    exit(0);
}

// Shoe table value or router behaviors
void show_table(int index, int show){

    table *current = head;
    unsigned char ip_bytes[4], mask = 0xff;

    // Information of connection
    if(show == 2){
        while(1){
            if(current->sd_index == index) break;
            current = current->next;
        }
        printf("socket index: %d\n", current->sd_index);

        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->pre_ip >> i*8) & mask;
        }
        printf("pre ip: %hhu.%hhu.%hhu.%hhu, pre port: %hu\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3], ntohs(current->pre_port));

        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->post_ip >> i*8) & mask;
        }
        printf("post ip: %hhu.%hhu.%hhu.%hhu, post port: %hu\n\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3], ntohs(current->post_port));
    }
    // Information of forward flow
    else if(show == 1){
        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->post_ip >> i*8) & mask;
        }
        printf("[%hhu.%hhu.%hhu.%hhu]\n\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    }
    // Information of backward flow
    else{
        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->pre_ip >> i*8) & mask;
        }
        printf("[%hhu.%hhu.%hhu.%hhu]\n\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    }

}

// Free the table
void free_table(table* current){
    if(current->next != NULL) free_table(current->next);
    free(current);
}

// Forward the packet to post or pre router
void forward(int from_sd, int index){

    table *current = head, *temp = head;
    char message[60];
    int bytes, to_sd, addrlen = sizeof(addr_client[index]);

    bzero(&message, sizeof(message));

    // Retrieve the message
    while(1){
        bytes = recvfrom(from_sd, message, sizeof(message), 0, (struct sockaddr *)&addr_client[index], &addrlen);
        if(bytes > 0) break;
    }

    // Find the corresponding tuple
    while(1){
        if(current->sd_index == index)break;
        current = current->next;
    }

    // If the socket index is odd, use even one to forward
    if(index % 2 == 1){

        // If the ip is matched
        if((addr_client[index]).sin_addr.s_addr == current->pre_ip){

            // If it is the first packet
            if(isset[index] == 0){

                // Update pre-port
                current->pre_port = (addr_client[index]).sin_port;
                while(temp->sd_index != index-1) temp = temp->next;
                temp->pre_port = current->pre_port;

                // Record
                isset[index-1] = 1;
                isset[index] = 1;
                if(TABLEUPDATE) printf("updated client port: %hu\n\n", ntohs(current->pre_port));
            }

            // Forward the packet
            bzero(&addr_temp, sizeof(addr_temp));
            addr_temp.sin_family = AF_INET;
            addr_temp.sin_addr.s_addr = current->post_ip;
            addr_temp.sin_port = current->post_port;
            bytes = sendto(client_socket[index - 1], message, bytes, 0, (struct sockaddr *)&addr_temp, sizeof(addr_temp));
        }
    }
    else{

        // Backward the packet
        bzero(&addr_temp, sizeof(addr_temp));
        addr_temp.sin_family = AF_INET;
        addr_temp.sin_addr.s_addr = current->pre_ip;
        addr_temp.sin_port = current->pre_port;
        bytes = sendto(client_socket[index + 1], message, bytes, 0, (struct sockaddr *)&addr_temp, sizeof(addr_temp));

    }
    // Show the behavior
    if(TABLEUPDATE){
        printf("packet from [%s] to ", inet_ntoa(((struct sockaddr_in *)&addr_client[index])->sin_addr));
        show_table(index, index % 2);
    }
}

// Bind the socket by its index
unsigned short bind_socket(int index){

    table *current = head;
    int len;

    // Bind the socket and let the system to choose an unused port number
    bzero(&addr_client[index], sizeof(addr_client[index]));
    (addr_client[index]).sin_family = AF_INET;
    (addr_client[index]).sin_addr.s_addr = addr_super.sin_addr.s_addr;
    (addr_client[index]).sin_port = 0;
    if(bind(client_socket[index], (struct sockaddr *)&addr_client[index], sizeof(addr_client[index])) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Retrieve the port number and return it
    bzero(&addr_temp, sizeof(addr_temp));
	len = sizeof(addr_temp);
	getsockname(client_socket[index], (struct sockaddr *) &addr_temp, &len);
    return ntohs(addr_temp.sin_port);
}

// Insert new tuples to the table
void insert(unsigned char *request, int index, int master_socket){

    table *current = (table*)malloc(sizeof(table)), *current2 = (table*)malloc(sizeof(table)), *temp;
    int bytes, len;
    unsigned short forward, backward;
    unsigned char new_request[length-8], ANS = 3, response[3];

    // Bind two sockets and retrieve bound ports
    forward = bind_socket(index);
    backward = bind_socket(index + 1);

    // Find the end of the table to insert
    if(head == NULL){
        current->next = current2;
        head = current;
    }
    else{
        temp = head;
        while (temp->next != NULL) {
            temp = temp->next;
        }
        temp->next = current;
        current->next = current2;
    }

    // Copy the payload to a new payload and update it
    new_request[0] = request[0] - 1;
    for(int i = 1; i < length-8; ++i) new_request[i] = request[i+8];

    // Fill up two tuples: current for forward and current2 for backward
    current->pre_ip = addr_request.sin_addr.s_addr;
    current->pre_port = addr_request.sin_port;
    current->post_ip = htonl((new_request[2] << 24) + (new_request[3] << 16) + (new_request[4] << 8) + new_request[5]);
    current->post_port = htons(new_request[7] + (new_request[8] << 8));
    current->sd_index = index;

    current2->pre_ip = current->pre_ip;
    current2->pre_port = current->pre_port;
    current2->post_ip = current->post_ip;
    current2->post_port = current->post_port;
    current2->sd_index = current->sd_index + 1;
    current2->next = NULL;

    // If the router itself is not the last one
    if(new_request[0] > 0){

        // Send the new payload to next router and wait for response (transit port)
        bzero(&addr_temp, sizeof(addr_temp));
        addr_temp.sin_family = AF_INET;
        addr_temp.sin_addr.s_addr = current->post_ip;
        addr_temp.sin_port = current->post_port;
        len = sizeof(addr_temp);
        bytes = sendto(client_socket[index], new_request, length-8, 0, (struct sockaddr *)&addr_temp, sizeof(addr_temp));
        bytes = recvfrom(client_socket[index], response, sizeof(response), 0, (struct sockaddr *)&addr_temp, &len);

        // If the response is ACK, update post-port
        if(bytes > 0 && response[0] == 3){
            current->post_port = htons(response[1] + (response[2] << 8));
            current2->post_port = current->post_port;
        }
        // Otherwise, continue the rejection to previous router
        else backward = 0;
    }

    // Respond its transit port to the client or previous router
    respond(client_socket[index + 1], backward);
}

// Respond transit port to the clien or previous router
void respond(int socket, unsigned short port){

    unsigned char payload[3], mask = 0xff;
    int bytes;

    // If port is 0, respond a rejection message
    if(port > 0) payload[0] = 3;
    else payload[0] = 0;
    payload[1] = port & mask;
    payload[2] = (port >> 8) & mask;

    bytes = sendto(socket, payload, 3, 0, (struct sockaddr *)&addr_request, sizeof(addr_request));
}

// Tunnel function
void tunnel(int master_socket){

    int bytes, sd, max_sd, clients = 0, activity, addrlen;
    unsigned short port;
    unsigned char request[400];
    fd_set readfds;

    // Handle ctrl-c
    signal(SIGINT, intHandler);

    // Init sockets
    for(int i = 0; i < MAXSOCKIND*2; ++i){
        client_socket[i] = 0;
    }

    // Keep monitoring packets
    while(1){

        // Clear all sockets in fd set and add master socket
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        // Add sockets and update max socket value
        for(int j = 0; j < MAXSOCKIND*2; ++j){
            sd = client_socket[j];
            if(sd > 0) FD_SET(sd, &readfds);
            if(sd > max_sd) max_sd = sd;
        }

        // Use select() to monitor avaliable I/O
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if(activity < 0 && (errno != EINTR)){
            perror("select");
        }

        // If master socekt is set, that means there is at least one connection request
        if(FD_ISSET(master_socket, &readfds)){

            // Retrieve the request
            addrlen = sizeof(addr_request);
            bzero(&request, sizeof(request));
            bytes = recvfrom(master_socket, request, sizeof(request), 0, (struct sockaddr *)&addr_request, &addrlen);
            length = bytes;

            // Print the source of the request
            if(TABLEUPDATE)
                printf("request from: %s\n\n", inet_ntoa(((struct sockaddr_in *)&addr_request)->sin_addr));

            // If the router can accept more clients
            if(clients < MAXSOCKIND * 2){

                // Build up 2 sockets for forward and backward
                client_socket[clients] = socket(AF_INET, SOCK_DGRAM, 0);
                client_socket[clients + 1] = socket(AF_INET, SOCK_DGRAM, 0);

                // Insert new tuples to the table
                insert(request, clients, master_socket);

                // Print the tuple in the table
                if(TABLEUPDATE) show_table(clients, 2);

                clients += 2;
            }
            // Reject the request otherwise
            else respond(master_socket, 0);
        }

        // Check if a socket is set
        for(int index = 0; index < MAXSOCKIND * 2; index++){
            sd = client_socket[index];
            if(FD_ISSET(sd, &readfds)){

                // Get the value and forward it
                forward(sd, index);
            }
        }
    }
}

int main(int argc, char *argv[]) {

    struct hostent *host_entry;
    table *current;
    char hostbuffer[256], *IPbuffer;
    int master_socket, enable = 1;

	// Build up master socket
	master_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (master_socket < 0) {
		perror("opening datagram socket");
		exit(1);
	}

    // Let the socket be able to receive multiple requests
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0 )
    {
        perror("setsockopt");
        exit(1);
    }

    // Retrieve hostname
    gethostname(hostbuffer, sizeof(hostbuffer));

    // Retrieve host information
    host_entry = gethostbyname(hostbuffer);

    // Convert an Internet network address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr *)host_entry->h_addr_list[0]));

	// Fill up local address
	bzero(&addr_super, sizeof(addr_super));
    addr_super.sin_family = AF_INET;
    addr_super.sin_addr.s_addr = inet_addr(IPbuffer);
    addr_super.sin_port = htons(atoi(argv[1]));

    // Bind master socket
    if(bind(master_socket, (struct sockaddr *)&addr_super, sizeof(addr_super)) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Start the router
    printf("router started.\n");
    tunnel(master_socket);

    return 0;
}