#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netdb.h>
#include <time.h>

#define MAX_SIZE 1488
#define second 1000000000

int check_state(FILE **file);
void client_thread(int tcp_fd, char *ip, int payload_size, int lambda, int mode, char *log);
void begin_server(unsigned short port, int payload_size, int lambda, int mode, char *log);
void transmit_audio(int udp_fd, unsigned int cli_ip, unsigned short cli_port, FILE *file, int payload_size, int lambda, int mode, char *log);
char **get_params(char *filename);
float cal_interval(int lambda, int mode, int payload_size, int gamma, int occupancy, int target, float alpha, float beta, float delta, float epsilon);

float cal_interval(int lambda, int mode, int payload_size, int gamma, int occupancy, int target, float alpha, float beta, float delta, float epsilon){

	float wait = second/lambda;

	if(occupancy < 0) return wait;

	// mode A
	if(mode == 0){
		if(occupancy == target) return wait;
		else if(occupancy < target) return wait - alpha;
		else return wait + alpha;
	}
	// mode B
	else if(mode == 1){
		if(occupancy == target) return wait;
		else if(occupancy < target) return wait - alpha;
		else return wait*delta;
	}
	// mode C
	else if(mode == 2){
		return wait + epsilon*(occupancy - target);
	}
	// mode D
	else return wait + epsilon*(occupancy - target) - beta*(wait - gamma*4096/payload_size);

}

char **get_params(char *filename){

	int bytes, index = 0;
	char dat_buffer[100], *ptr, **params = (char**)malloc(4*sizeof(char*));
	FILE *param = fopen(filename, "r");

	if(param == NULL){
		perror("fopen");
		exit(1);
	}
	else{
		bzero(dat_buffer, sizeof(dat_buffer));
		bytes = fread(dat_buffer, 1, sizeof(dat_buffer), param);
		if(bytes < 8){
			perror("fread");
			exit(1);
		}
		ptr = strtok(dat_buffer," ");
		while(ptr != NULL){
			if(index == 4) break;
			params[index] = ptr;
			ptr = strtok(NULL, " ");
			index++;
		}
		return params;
	}
}

void transmit_audio(int udp_fd, unsigned int cli_ip, unsigned short cli_port, FILE *file, int payload_size, int lambda, int mode, char *log){

	struct sockaddr_in cli_addr;
	struct timespec interval;
	struct timeval tv_recv, tv_start, tv_end;
	int bytes, len = sizeof(cli_addr), total = 0, duration;
	int gamma = 0, occupancy = -1, target = 0;
	float alpha, beta, delta, epsilon, wait;
	char **params, feedback[12];

	// struct sigaction feedback;
	// feedback.sa_flags = 0;
	// feedback.sa_handler = io_handler;
	// sigaction(SIGIO, &feedback, NULL);
	// Set timer for recvfrom.
    tv_recv.tv_sec = 1;
    tv_recv.tv_usec = 1;
    setsockopt(udp_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_recv, sizeof(struct timeval));

	bzero(&cli_addr, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = cli_ip;
	cli_addr.sin_port = cli_port;

	**params = get_params("control-param.dat");
	sscanf(params[0], "%f", &alpha);
	sscanf(params[1], "%f", &delta);
	sscanf(params[2], "%f", &epsilon);
	sscanf(params[3], "%f", &beta);
	free(params);

	payload_size = payload_size < MAX_SIZE ? payload_size : MAX_SIZE;
	char send_buffer[payload_size];
	bzero(send_buffer, sizeof(send_buffer));
	while((bytes = fread(send_buffer, 1, payload_size, file))>0){

		wait = cal_interval(lambda, mode, payload_size, gamma, occupancy, target, alpha, beta, delta, epsilon);
		lambda = (int)second / wait;
		tv_recv.tv_sec = (int)wait / second;
		tv_recv.tv_usec = (int)(wait % second)/1000;
		bytes = sendto(udp_fd, send_buffer, strlen(send_buffer), 0, (struct sockaddr *)&cli_addr, sizeof(cli_addr));

		total += bytes;
		bzero(&feedback, sizeof(feedback));
		gettimeofday(&tv_start, NULL);
		bytes = recvfrom(udp_fd, feedback, sizeof(feedback), 0, (struct sockaddr *)&cli_addr, &len);
		gettimeofday(&tv_end, NULL);
		if(bytes == 12){
			occupancy = target = gamma = 0;
			for(int i = 0; i < 4; ++i){
				occupancy += feedback[i] << 8*i;
				target += feedback[i+4] << 8*i;
				gamma += feedback[i+8] << 8*i;
			}
		}
		duration = wait - (tv_end.tv_sec - tv_start.tv_sec + (tv_end.tv_usec - tv_start.tv_usec)*1000);
		interval.tv_sec = (int)duration / second;
		interval.tv_nsec = (int)duration % second;
		nanosleep(&interval, NULL);
		bzero(&send_buffer, sizeof(send_buffer));
	}
	fclose(file);
}

void client_thread(int tcp_fd, char *ip, int payload_size, int lambda, int mode, char *log){

	FILE *file;
	struct sockaddr_in srv_addr, tmp_addr;
    int udp_fd, control, addrlen = sizeof(address), len;
	char path[150], filename[100];
	unsigned char tcp_buffer[7];
	unsigned short udp_port, cli_port;
	unsigned int file_size;

	// Wait for the filename
	bzero(&filename, sizeof(filename));
	read(tcp_fd, filename, sizeof(filename));

	// Open the file and check its state
	bzero(&path, sizeof(path));
	sprintf(path, "/tmp/%s", filename);
	file = fopen(path,"r");

	if(file == NULL){
		memset(tcp_buffer, '0', sizeof(tcp_buffer));
		write(tcp_fd, tcp_buffer, sizeof(tcp_buffer));
		close(tcp_fd);
	}
	else{

		fseek(file, 0, SEEK_END);
		file_size = ftell(file);
		printf("file size: %u\n", file_size);
		fseek(file, 0, SEEK_SET);

		udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
		if(udp_fd < 0){
			perror("socket");
			exit(1);
		}

		bzero(&srv_addr, sizeof(srv_addr));
		srv_addr.sin_family = AF_INET;
		srv_addr.sin_addr.s_addr = inet_addr(ip);
		srv_addr.sin_port = 0;

		if(bind(udp_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0){
			perror("bind udp");
			exit(1);
    	}

		bzero(&tmp_addr, sizeof(tmp_addr));
		len = sizeof(tmp_addr);
		getsockname(udp_fd, (struct sockaddr *) &tmp_addr, &len);
		udp_port = tmp_addr.sin_port;

		tcp_buffer[0] = '2';
		tcp_buffer[1] = udp_port & 0xff;
		tcp_buffer[2] = (udp_port >> 8) & 0xff;

		for(int i = 0; i < 4; ++i){
			tcp_buffer[3+i] = (file_size >> 8*i) & 0xff;
		}
		write(tcp_fd, tcp_buffer, sizeof(tcp_buffer));
		bzero(&tcp_buffer, sizeof(tcp_buffer));
		read(tcp_fd, &tcp_buffer, sizeof(tcp_buffer));
		cli_port = tcp_buffer[0] + tcp_buffer[1] << 8;

		bzero(&tmp_addr, sizeof(tmp_addr));
		socklen_t addr_size = sizeof(struct sockaddr_in);
		getpeername(tcp_fd, (struct sockaddr *)&tmp_addr, &addr_size);
		transmit_audio(udp_fd, tmp_addr.sin_addr.s_addr, cli_port, file, payload_size, lambda, mode, log);

		bzero(&tcp_buffer, sizeof(tcp_buffer));
		tcp_buffer[0] = '5';
		write(tcp_fd, &tcp_buffer, sizeof(tcp_buffer));

		//TODO: need to write log to logfile
	}
	// Shutdown and close
	// shutdown(fd, SHUT_WR);
	// if(read(fd, file_buffer, MAXBUFSZM) == 0) close(fd);
}

void begin_server(unsigned short port, int payload_size, int lambda, int mode, char *log){

	struct sockaddr_in address;
	struct hostent *host_entry;
	int fd, new_fd, addrlen = sizeof(address);
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

	if((bind(fd, (struct sockaddr *)&address, sizeof(address))) != 0){
		perror("bind tcp");
		exit(1);
	}
	if((listen(fd,5)) != 0){
		perror("listen");
		exit(1);
	}
	while(1){
		new_fd = accept(fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
		if(new_fd < 0) continue;
		k = fork();
		if(k == 0) client_thread(new_fd, IPbuffer, payload_size, lambda, mode, log);
	}
}

int main(int argc, char *argv[])
{

	unsigned short tcp_port = atoi(argv[1]);
	int payload_size = atoi(argv[2]);
	int lambda = atoi(argv[3]);
	int mode = atoi(argv[4]);
	char *logfile = argv[5];

	// Start the process
    begin_server(tcp_port, payload_size, lambda, mode, logfile);

	return 0;
}
