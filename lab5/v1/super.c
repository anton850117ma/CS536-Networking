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
#include "supergopher.h"

#define MAXSOCKIND 10

// Table structure
typedef struct forwarding_table{
    unsigned short t_port, t_port2, cli_port, srv_port;
    unsigned int sd_index, cli_ip, srv_ip;
    struct forwarding_table *next;
}table;

table *head = NULL;
struct sockaddr_in addr_super, addr_request, addr_temp, addr_client[MAXSOCKIND*2];
int client_socket[MAXSOCKIND*2];

unsigned short bind_socket(int socket_from, int socket_to, int index);
void forward(int sd, int index);
void insert(unsigned char *request, int index);
void tunnel(int master_socket);
void respond(int socket, unsigned short port);
void free_table(table* current);
void show_table(int index, int show);;
void intHandler(int dummy);

// Handle ctrl-c to free the table and close sockets
void intHandler(int dummy){

    free_table(head);
    for(int index = 0; index < MAXSOCKIND*2; ++index) close(client_socket[index]);
    exit(0);
}

// Show the table
void show_table(int index, int show){

    table *current = head;
    unsigned char ip_bytes[4], mask = 0xff;

    // Information of the connection
    if(show == 2){
        while(1){
            if(current->sd_index == index) break;
            current = current->next;
        }
        printf("socket index: %d\n", current->sd_index);

        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->cli_ip >> i*8) & mask;
        }
        printf("client ip: %hu.%hu.%hu.%hu, client port: %hu\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3], ntohs(current->cli_port));

        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->srv_ip >> i*8) & mask;
        }
        printf("server ip: %hu.%hu.%hu.%hu, server port: %hu\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3], ntohs(current->srv_port));
        printf("transit port: %hu, transit port2: %hu\n\n", ntohs(current->t_port), ntohs(current->t_port2));
    }
    // Information of forward flow
    else if(show == 0){
        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->srv_ip >> i*8) & mask;
        }
        printf("[%hhu.%hhu.%hhu.%hhu]\n\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    }
    // Information of backward flow
    else{
        for(int i = 0; i < 4; ++i){
            ip_bytes[i] = (current->cli_ip >> i*8) & mask;
        }
        printf("[%hhu.%hhu.%hhu.%hhu]\n\n", ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
    }

}

// Free the table
void free_table(table* current){
    if(current->next != NULL) free_table(current->next);
    free(current);
}

// Forward packets to the client or the server
void forward(int from_sd, int index){

    table *current = head;
    char message[60];
    int bytes, to_sd, addrlen = sizeof(addr_client[index]);

    bzero(&message, sizeof(message));

    // Retrieve the packet
    while(1){
        bytes = recvfrom(from_sd, message, sizeof(message), 0, (struct sockaddr *)&addr_client[index], &addrlen);
        if(bytes > 0) break;
    }

    // Find the corresponding tuple
    while(1){
        if(current->sd_index == index)break;
        current = current->next;
    }

    // If the packet is from the client
    if(index % 2 == 0){

        // If the ip is matched
        if((addr_client[index]).sin_addr.s_addr == current->cli_ip){

            // If client's port is a dummy value (0), update it
            if(current->cli_port == 0){
                current->cli_port = (addr_client[index]).sin_port;
                current->next->cli_port = (addr_client[index]).sin_port;
                if(TABLEUPDATE) printf("updated client port: %hu\n\n", ntohs(current->cli_port));
            }

            // Forward the packet to the server
            bzero(&addr_temp, sizeof(addr_temp));
            addr_temp.sin_family = AF_INET;
            addr_temp.sin_addr.s_addr = current->srv_ip;
            addr_temp.sin_port = current->srv_port;
            bytes = sendto(client_socket[index + 1], message, bytes, 0, (struct sockaddr *)&addr_temp, sizeof(addr_temp));
        }
    }
    else{
        // Backward the packet to the client
        bzero(&addr_temp, sizeof(addr_temp));
        addr_temp.sin_family = AF_INET;
        addr_temp.sin_addr.s_addr = current->cli_ip;
        addr_temp.sin_port = current->cli_port;
        bytes = sendto(client_socket[index - 1], message, bytes, 0, (struct sockaddr *)&addr_temp, sizeof(addr_temp));
    }
    // Show router behavior
    if(TABLEUPDATE){
        printf("packet from [%s] to ", inet_ntoa(((struct sockaddr_in *)&addr_client[index])->sin_addr));
        show_table(index, index % 2);
    }
}

// Bind two sockets
unsigned short bind_socket(int socket_from, int socket_to, int index){

    table *current = head;
    int len;

    // Find the corresponding tuple
    while (1) {
        if(current->sd_index == index) break;
        current = current->next;
    }

    // Bind the forward socket
    bzero(&addr_client[index], sizeof(addr_client[index]));
    (addr_client[index]).sin_family = AF_INET;
    (addr_client[index]).sin_addr.s_addr = addr_super.sin_addr.s_addr;
    (addr_client[index]).sin_port = 0;
    if(bind(socket_from, (struct sockaddr *)&addr_client[index], sizeof(addr_client[index])) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Retrieve transit port to store and return
    bzero(&addr_temp, sizeof(addr_temp));
	len = sizeof(addr_temp);
	getsockname(socket_from, (struct sockaddr *) &addr_temp, &len);
    current->t_port = addr_temp.sin_port;
    current->next->t_port = addr_temp.sin_port;

    // Bind the backward socket
    bzero(&addr_client[index + 1], sizeof(addr_client[index + 1]));
    (addr_client[index + 1]).sin_family = AF_INET;
    (addr_client[index + 1]).sin_addr.s_addr = addr_super.sin_addr.s_addr;
    (addr_client[index + 1]).sin_port = 0;
    if(bind(socket_to, (struct sockaddr *)&addr_client[index + 1], sizeof(addr_client[index + 1])) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Retrieve transit port 2 to store
    bzero(&addr_temp, sizeof(addr_temp));
	len = sizeof(addr_temp);
	getsockname(socket_to, (struct sockaddr *) &addr_temp, &len);
    current->t_port2 = addr_temp.sin_port;
    current->next->t_port2 = addr_temp.sin_port;

    return ntohs(current->t_port);
}

// Insert a tuple to the table
void insert(unsigned char *request, int index){

    // If the head of the table is null
    if(head == NULL){
        head = (table*)malloc(sizeof(table));
        head->cli_ip = addr_request.sin_addr.s_addr;
        head->cli_port = 0;
        head->srv_ip = htonl((request[0] << 24) + (request[1] << 16) + (request[2] << 8) + request[3]);
        head->srv_port = htons(request[4] + (request[5] << 8));
        head->sd_index = index;
        head->next = NULL;

    }
    // Find the last tuple in the table otherwise
    else{
        table *current = head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = (table*)malloc(sizeof(table));
        current->next->cli_ip = addr_request.sin_addr.s_addr;
        current->next->cli_port = 0;
        current->next->srv_ip = htonl((request[0] << 24) + (request[1] << 16) + (request[2] << 8) + request[3]);
        current->next->srv_port = htons(request[4] + (request[5] << 8));
        current->next->sd_index = index;
        current->next->next = NULL;
    }
}

// Respond transit port to the clien
void respond(int socket, unsigned short port){

    unsigned char payload[3], mask = 0xff;
    int bytes;

    // If port is 0, respond a rejection message
    if(port > 0) payload[0] = '3';
    else payload[0] = '0';
    payload[1] = port & mask;
    payload[2] = (port >> 8) & mask;

    bytes = sendto(socket, payload, strlen(payload), 0, (struct sockaddr *)&addr_request, sizeof(addr_request));
}

// Tunnel function
void tunnel(int master_socket){

    int bytes, sd, max_sd, clients = 0, activity, addrlen;
    unsigned short port;
    unsigned char request[6];
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

            // Print the source of the request
            if(TABLEUPDATE)
                printf("request from: %s\n\n", inet_ntoa(((struct sockaddr_in *)&addr_request)->sin_addr));

            // If the vpn can accept more clients
            if(clients < MAXSOCKIND * 2){

                // Bind two sockets and insert two tuples and bind those sockets
                client_socket[clients] = socket(AF_INET, SOCK_DGRAM, 0);
                client_socket[clients + 1] = socket(AF_INET, SOCK_DGRAM, 0);
                insert(request, clients);
                insert(request, clients + 1);
                port = bind_socket(client_socket[clients], client_socket[clients + 1], clients);

                // Show the tuple
                if(TABLEUPDATE) show_table(clients, 2);

                // Update client number and respond transit port
                clients += 2;
                respond(master_socket, port);
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

    // Start vpn
    printf("vpn started.\n");
    tunnel(master_socket);

    return 0;
}