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

enum status{init_ses_no_req = 1, init_ses_to_res, init_ses_to_sen, init_ses_wait_res, chat_ses};
struct sockaddr_in addr_me, addr_other, addr_part, addr_first;
struct timeval tv_recv;
int fd, addrlen, cond, bytes, port_other, resend, disconnect, first = 0;
char req_buffer[50], tmp_buffer[50], ready_me[50], ready_other[60], *ip_other, *payload;

void terve_quit(int sig);
void sigalarmHandler(int sig);
void terve_msg_receive(int sig);
void store_other();
void send_request();
void begin_to_chat();
char **parse(char *buffer);
char *gen_payload(int msg);
int compare(char *str1, char *str2, int maxsize);

// Store the specific remote party we want to chat with
void store_other(){

    // Store the ip and port number for the first request
    if(1 == first){
        bzero(&addr_first, sizeof(addr_first));
        addr_first.sin_family = AF_INET; 
        addr_first.sin_addr.s_addr = addr_other.sin_addr.s_addr;
        addr_first.sin_port = addr_other.sin_port;
    }

    // Store the ip and port number for chatting at host which receives the request
    else if(2 == first){
        bzero(&addr_part, sizeof(addr_part));
        addr_part.sin_family = AF_INET; 
        addr_part.sin_addr.s_addr = addr_first.sin_addr.s_addr;
        addr_part.sin_port = addr_first.sin_port;
    }
    
}

// Compare two char arrays
int compare(char *str1, char *str2, int maxsize){

    int result = 1;
    char* new_string1 = (char*)malloc(maxsize*sizeof(char));
    char* new_string2 = (char*)malloc(maxsize*sizeof(char));

    strncpy(new_string1,str1,maxsize);
    strncpy(new_string2,str2,maxsize);
    
    if(strcmp(new_string1,new_string2)==0) result = 0;
    
    free(new_string1);
    free(new_string2);
    return result;
}

// Generate a random payload with a specified control number
char* gen_payload(int msg){
    
    int number = (rand() % (9999 - 1000 + 1)) + 1000;
    char* tempload = (char*)malloc(5*sizeof(char));
    sprintf(tempload, "%d%d", msg, number);
    return tempload;
}

// Parse ip address and port number with given user input
char **parse(char *buffer){

    int count = 0;
    char *pch, *temp = (char*)malloc(strlen(buffer)*sizeof(char));
    char **command = (char**)malloc(30*sizeof(char*));

    memset(command, '\0', sizeof(char*) * 30);
    strncpy(temp, buffer, strlen(buffer));

	pch = strtok(temp," ");
    while(pch != NULL){
     	command[count] = pch;
      	pch = strtok(NULL," ");
     	count++;
    }
    return command;
}

// Handler for SIGQUIT
void terve_quit(int sig){

    // If the condition is chat session
    if(cond == chat_ses){

        // Send a terminating message to the other side
        req_buffer[0] = '9';
        bytes = sendto(fd, req_buffer, strlen(req_buffer), 0, (struct sockaddr *)&addr_part, sizeof(addr_part));
    }
    // If the condition is waiting for response
    // This is triggered when local party terminates after sending requests during a initial session.
    else if(cond == init_ses_wait_res){

        // Tell the remote party to ignore the request sent from here
        payload[0] = '9';
        send_request();
    }
    close(fd);
    exit(0);
}

// Handler for SIGALRM
void sigalarmHandler(int sig){
    
    // If the condition is waiting for response
    if(cond == init_ses_wait_res){

        // Resend local request within 2 times
        if(resend < 2){
            alarm(5);
            send_request();
        }
        resend++;
    }
    // If the condtion is about to response
    // This is triggered when user does not decide yes or no after three transmissions in total
    else if(cond == init_ses_to_res){

        // Wait for user input again
        first = 0;
        cond = init_ses_no_req;
    }
    // If the condition is chat session
    // This is triggered when a long silence occurs
    else if(cond == chat_ses){

        // Disconnect and wait for user input again
        disconnect = 1;
        cond = init_ses_no_req;
    }
}

// Handler for SIGIO
void terve_msg_receive(int sig){

    // If the condition is about to send a local request
    if(cond == init_ses_to_sen){
        
        // Receive request from other hosts
        bzero(req_buffer, sizeof(req_buffer));
	    bytes = recvfrom(fd, req_buffer, sizeof(req_buffer), 0, (struct sockaddr *)&addr_other, &addrlen);
        if(bytes > 0){
            // If the first value is 9, that means the host in the other side is terminated
            // Thus, wait for user inputs for another connection
            if(req_buffer[0] == '9'){
                cond = init_ses_no_req;
            }
            // If the first value is 5, it is considered as a remote request.
            // Thus, show the message and wait for user's decision
            else if(req_buffer[0] == '5'){

                // Collect remote ip address and port number to show
                ip_other = inet_ntoa(addr_other.sin_addr);
                port_other = ntohs(addr_other.sin_port);
                printf("\n#session request from: %s %d\n#ready:", ip_other, port_other);
                fflush(stdout);

                // Go to responding condion and set the timer for the maximum responding time
                first = 1;
                store_other();
                cond = init_ses_to_res;
                alarm(15);
            }
        }
    }
    // If the condition is about to response a remote request 
    else if(cond == init_ses_to_res){

        bzero(tmp_buffer, sizeof(tmp_buffer));
	    bytes = recvfrom(fd, tmp_buffer, sizeof(tmp_buffer), 0, (struct sockaddr *)&addr_other, &addrlen);
        if(bytes > 0){

            // If the host that initniate the chat session is terminated
            if(tmp_buffer[0] == '9'){

                // Show the message and go back to wait user input
                printf("\n#host from %s is terminated.\n#ready:", ip_other);
                fflush(stdout);
                cond = init_ses_no_req;
            }
            // If the remote request is a retransmitted request
            else if (tmp_buffer[0] == '5'){
                ip_other = inet_ntoa(addr_other.sin_addr);
                port_other = ntohs(addr_other.sin_port);
                printf("\n#session request from: %s %d\n#ready:", ip_other, port_other);
                fflush(stdout);
            }
            // If the host which init session request gives up, reset clock and go back to no request condition
            else if(tmp_buffer[0] == '4'){
                alarm(0);
                cond = init_ses_no_req;
            }
        }
    }
    // If the condition is chat session
    else if(cond == chat_ses){

        bzero(ready_other, sizeof(ready_other));
	    bytes = recvfrom(fd, ready_other, sizeof(ready_other), 0, (struct sockaddr *)&addr_other, &addrlen);

        if(bytes > 0){

            // If the message is a remote request, show it and send a rejecting message back to avoid future interrupts (visually)
            if(ready_other[0] == '5'){
                ready_other[0] = '4';
                ip_other = inet_ntoa(addr_other.sin_addr);
                port_other = ntohs(addr_other.sin_port);
                printf("\n#session request from: %s %d\n#your msg:", ip_other, port_other);
                fflush(stdout);
                bytes = sendto(fd, ready_other, strlen(ready_other), 0, (struct sockaddr *)&addr_other, sizeof(addr_other));
            }
            // If the message is a chatting message 
            else if(ready_other[0] == '8'){

                // Reset the timer which is for detecting a long silence
                alarm(0);

                // Extract the message in the entire payload 
                char temp_buf[60];
                bzero(temp_buf, sizeof(temp_buf));
                payload = ready_other;
                memcpy(temp_buf, &payload[5], strlen(payload)-5);
                printf("\n#received msg: %s\n#your msg:", temp_buf);
                fflush(stdout);

                // Set the timer again (3 mins)
                alarm(180);
            }
            // If the message is a terminating message, then terminate.
            else if(ready_other[0] == '9'){
                printf("\n#session termination received\n");
                close(fd);
                exit(0);
            }
        }
    }
}

// Send local request with user input
void send_request(){


    char **ip_port;

    // Parse user input (only accepts valid input)
    ip_port = parse(ready_me);

    // Send the request
    bzero(&addr_part, sizeof(addr_part));
    addr_part.sin_family = AF_INET; 
    addr_part.sin_addr.s_addr = inet_addr(ip_port[0]);
    addr_part.sin_port = htons(atoi(ip_port[1]));
    bytes = sendto(fd, payload, strlen(payload), 0, (struct sockaddr *)&addr_part, sizeof(addr_part));

    free(ip_port);
}

void begin_to_chat(){

    int flags;
    char char_y[] = "y", char_n[] = "n", msg_me[50], package[60];

    // Register SIGIO handler and set the socket (fd) as I/O asynchronized
    signal(SIGIO, terve_msg_receive);
	if (fcntl(fd, F_SETOWN, getpid()) < 0){
		perror("fcntl F_SETOWN");
		exit(1);
	}
	if (fcntl(fd, F_SETFL,FASYNC) < 0){
		perror("fcntl F_SETFL, FASYNC");
		exit(1);
	}

    // Register SIGALRM and SIGQUIT handler
    signal(SIGALRM, sigalarmHandler);
    signal(SIGQUIT, terve_quit);

    // Set timer for recvfrom.
    tv_recv.tv_sec = 5;
    tv_recv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv_recv, sizeof(struct timeval));
    
    // Start from no-request condition
    cond = init_ses_no_req;
    addrlen = sizeof(addr_other);

    while(1){

        // Initial session without a remote request
        if(cond == init_ses_no_req){

            // Go to send local request with user input
            cond = init_ses_to_sen;
            printf("#ready:");
            fflush(stdout);
            fgets(ready_me, 50, stdin);
            ready_me[strlen(ready_me)-1] = '\0';
            
        }
        // Initial session with sending local request
        else if(cond == init_ses_to_sen){

            // Go to wait for remote response
            cond = init_ses_wait_res;
            payload = gen_payload(5);
            resend = 0;

            // Set the timer for the request
            alarm(5);
            send_request(); 

        }
        // Initial session with waiting remote response
        else if(cond == init_ses_wait_res){
            
            bzero(req_buffer, sizeof(req_buffer));

            // Recevie any messages within 5 seconds. 
            while(1){
                bytes = recvfrom(fd, req_buffer, sizeof(req_buffer), 0, (struct sockaddr *)&addr_other, &addrlen);
                
                // To deal with -1 condition triggered by sendto() in SIGALRM handler
                if(resend < 3 && bytes < 0) continue;
                break;
            }
            
            // If the message is a remote request, send a message back to avoid further retransmission.
            if(bytes > 0 && req_buffer[0] == '5'){
                req_buffer[0] = '4';
                ip_other = inet_ntoa(addr_other.sin_addr);
                port_other = ntohs(addr_other.sin_port);
                printf("\n#session request from: %s %d\n", ip_other, port_other);
                bytes = sendto(fd, req_buffer, strlen(req_buffer), 0, (struct sockaddr *)&addr_other, sizeof(addr_other));
                continue;
            }
            // If the message is "Let's talk"
            else if(bytes > 0 && req_buffer[0] == '6'){

                req_buffer[0] = '5';
                // Check the consistency of payloads
                if(compare(req_buffer, payload, 30) == 0){

                    // Reset the timer
                    alarm(0);
                    printf("\n#success: %s\n", ready_me);

                    // Store the remote party for chat session
                    disconnect = 0;

                    // Go to chat session
                    cond = chat_ses;

                    // Set the timer for silence
                    alarm(180);
                    continue;
                }
            }

            // Reset the timer and give up
            alarm(0);
            req_buffer[0] = '4';
            bytes = sendto(fd, req_buffer, strlen(req_buffer), 0, (struct sockaddr *)&addr_part, sizeof(addr_part));
            printf("\n#failure: %s\n", ready_me);
            cond = init_ses_no_req;
        }
        // Initial session with sending response
        else if(cond == init_ses_to_res){
            
            bytes = strlen(ready_me);

            // User accepts the remote request
            if(bytes == 1 && ready_me[0] == 'y'){
                alarm(0);
                req_buffer[0] = '6';
                while(1){
                    bytes = sendto(fd, req_buffer, strlen(req_buffer), 0, (struct sockaddr *)&addr_first, sizeof(addr_first));
                    if(bytes == strlen(req_buffer)) break;
                }
                first = 2;
                store_other();
                disconnect = 0;
                cond = chat_ses;
                alarm(180);
            }
            // User rejects the remote request
            else if(bytes == 1 && ready_me[0] == 'n'){
                alarm(0);
                req_buffer[0] = '7';
                while(1){
                    bytes = sendto(fd, req_buffer, strlen(req_buffer), 0, (struct sockaddr *)&addr_first, sizeof(addr_first));
                    if(bytes == strlen(req_buffer)) break;
                }
                first = 0;
                cond = init_ses_no_req;
            }
            // Waiting user to give valid input
            else{
                printf("#ready:");
                fflush(stdout);
                fgets(ready_me, 50, stdin);
                ready_me[strlen(ready_me)-1] = '\0';
            }

        }
        // Chat session
        else if(cond == chat_ses){
            
            // Get user message
            printf("#your msg:");
            fflush(stdout);
            memset(msg_me, '\0', sizeof(msg_me));
            read(STDIN_FILENO, msg_me, sizeof(msg_me));
            // If encounter a long silence, disconnect.
            if(disconnect) continue;

            // Send the message 
            req_buffer[0] = '8';
            sprintf(package, "%s%s", req_buffer, msg_me);
            bytes = sendto(fd, package, strlen(package), 0, (struct sockaddr *)&addr_part, sizeof(addr_part));

        }
    }
    close(fd);
}

int main(int argc, char *argv[]) {


	struct hostent *host_entry;
    char hostbuffer[256], *IPbuffer;

    srand(time(NULL));

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

	// Fill up address structure
	bzero(&addr_me, sizeof(addr_me));
    addr_me.sin_family = AF_INET; 
    addr_me.sin_addr.s_addr = inet_addr(IPbuffer); 
    addr_me.sin_port = htons(atoi(argv[1]));

    // Bind the socket
    if(bind(fd, (struct sockaddr *)&addr_me, sizeof(addr_me)) < 0){
        perror("binding datagram socket");
		exit(1);
    }

    // Begin to chat
    begin_to_chat();
     
    return 0; 
}