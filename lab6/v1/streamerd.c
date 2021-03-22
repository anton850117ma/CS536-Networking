/*******************************************************************************
*  Author : Xiao Wang
*  Email  : wang3702@purdue.edu
*******************************************************************************/

#include <termios.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/time.h>
#include <ctype.h>
#include <ifaddrs.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <poll.h>
#include <pthread.h>
#include "debug.h"
#define MAXLEN		65538 /* Max Command execution output characters to be written */
#define MAX_LISTEN	10
int lambda;
int control_mode;
float a_factor;
float delta_factor;
float epsilon_factor;
float beta_factor;
int udp_socket;
int transfer_label=0;
struct sockaddr_in send_to_addr;
int	send_to_addrLen = sizeof(send_to_addr);
int lambda_log[MAXLEN][2];
int lambda_count=0;
int payload_size;
char **explode(char sep, const char *str, int *size)
{
        int count = 0, i;
        for(i = 0; i < strlen(str); i++)
        {
                if (str[i] == sep)
                {
                        count ++;
                }
        }

        char **ret = calloc(++count, sizeof(char *));

        int lastindex = -1;
        int j = 0;

        for(i = 0; i < strlen(str); i++)
        {
                if (str[i] == sep)
                {
                        ret[j] = calloc(i - lastindex, sizeof(char)); //allocate space
                        memcpy(ret[j], str + lastindex + 1, i - lastindex - 1);
                        j++;
                        lastindex = i;
                }
        }
        //deal with the final substr
        if (lastindex <= strlen(str) - 1)
        {
                ret[j] = calloc(strlen(str) - lastindex, sizeof(char));
                memcpy(ret[j], str + lastindex + 1, strlen(str) - 1 - lastindex);
                j++;
        }

        *size = j;

        return ret;
}

static char *getip()
{
	struct ifaddrs	*ifaddr, *ifa;
	int		family, s;
	char		*host = NULL;

	if ( getifaddrs( &ifaddr ) == -1 )
	{
		perror( "getifaddrs" );
		return(NULL);
	}

	for ( ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next )
	{
		if ( ifa->ifa_addr == NULL )
			continue;

		family = ifa->ifa_addr->sa_family;

		if ( !strcmp( ifa->ifa_name, "lo" ) )
			continue;
		if ( family == AF_INET )
		{
			if ( (host = malloc( NI_MAXHOST ) ) == NULL )
				return(NULL);
			s = getnameinfo( ifa->ifa_addr,
					 (family == AF_INET) ? sizeof(struct sockaddr_in) :
					 sizeof(struct sockaddr_in6),
					 host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST );
			if ( s != 0 )
			{
				return(NULL);
			}
			freeifaddrs( ifaddr );
			return(host);
		}
	}
	return(NULL);
}
int calculate_file_size(char* filename)
{
    struct stat statbuf;
    stat(filename,&statbuf);
    int size=statbuf.st_size;

    return size;
}
void mssleep(int const time_in_ms)
{
    struct timespec time;
    struct timespec time_buf;

    int ret = -1;
    time.tv_sec = (time_in_ms / 1000000000);
    time.tv_nsec = (time_in_ms-(time_in_ms / 1000000000));
    //printf("Seconds %d,nano %d",time.tv_sec,time.tv_nsec);
    time_buf = time;
    while(1 == 1) /* lint warning modified */
    {
        time = time_buf;
        ret = nanosleep(&time, &time_buf);
        if((ret < 0))
		{
			continue;
			printf("nano sleep call failed\n");
		}
        else
            break;
    }
    return;
}
char buf_chat[MAXLEN];
int buf_chat_len=MAXLEN;
int update_lambda(int signo)  /*deal with the info to update lambda*/
{
	//printf("Calling the receive sigio\n");
	//fflush(stdout);
	if(transfer_label==0){

		bzero(buf_chat,MAXLEN);
    	int len= recvfrom(udp_socket, buf_chat, buf_chat_len, 0, &send_to_addr, &send_to_addrLen);
    	buf_chat[len]='\0';
    	//verify the port
    	printf( "Confirmed!!sending udp packet to address = %s:%d\n", inet_ntoa( send_to_addr.sin_addr ), ntohs(send_to_addr.sin_port ) );
    	fflush(stdout);
    	int check_port=ntohs(send_to_addr.sin_port );
    	int extract_size;
    	char **connect_info = explode(' ', buf_chat, &extract_size);
    	if(extract_size!=2){
    		printf("Confirming message is not correct, we following [confirm_id] [client_udp_port]\n");
    		fflush(stdout);
    		return;
    	}
    	if(strcmp(connect_info[0],"99")!=0){
    		printf("Confirming message is not correct, we following [confirm_id] [client_udp_port] and used 99 as confirm id\n");
    		fflush(stdout);
    		return;
    	}
    	int pass_port=atoi(connect_info[1]);
    	if(check_port==pass_port){
    		printf("Confirm All info correct, start udp transmission\n");
    		fflush(stdout);
    		transfer_label=1;
    		return;
    	}
	}else{
		//accept the info from client to update lambda
		struct sockaddr_in send_to_addr1;
		int	send_to_addrLen1 = sizeof(send_to_addr1);
		bzero(buf_chat,MAXLEN);
    	int len= recvfrom(udp_socket, buf_chat, buf_chat_len, 0, &send_to_addr1, &send_to_addrLen1);
    	buf_chat[len]='\0';
    	int extract_size;
    	//get all the information
    	char **connect_info= explode(' ', buf_chat, &extract_size);
    	//printf(buf_chat);
    	//fflush(stdout);
    	int buffer_occupancy=atoi(connect_info[0]);
    	int target_buf=atoi(connect_info[1]);
    	int gamma_factor=atoi(connect_info[2]);
    	if(DEBUG){
    	printf("buffer_occupancy %d, targt buffer %d,gamma_factor %d\n",buffer_occupancy,target_buf,gamma_factor);
    	fflush(stdout);}
    	int prev_lambda=lambda;
    	//float wait_time;
    	//then based on the control mode to calculate new lambda
    	if(control_mode==0){
    		// A method
    		if(buffer_occupancy==target_buf){
    			lambda=lambda;
    		}else if(buffer_occupancy<target_buf){

    			lambda=lambda+a_factor;
    		}else{
    			lambda=lambda-a_factor;
    		}
    	}else if(control_mode==1){
    		// B method
    		if(buffer_occupancy==target_buf){
    			lambda=lambda;
    		}else if(buffer_occupancy<target_buf){
    			lambda=lambda+a_factor;
    		}else{
    			lambda=(int)(delta_factor*lambda);
    		}

    	}else if(control_mode==2){
    		//C method
    		lambda=lambda+(int)(epsilon_factor*(target_buf-buffer_occupancy)/(float)payload_size);
    	}else if(control_mode==3){
			// lambda=lambda+(int)(epsilon_factor*(target_buf-buffer_occupancy)/(float)payload_size)-(int)(beta_factor*(lambda-gamma_factor*4096/(float)payload_size));
			lambda=lambda+(int)(epsilon_factor*(target_buf-buffer_occupancy)/(float)payload_size-beta_factor*(lambda-gamma_factor*4096/(float)payload_size));
    	}
    	if(lambda<=0){
    		lambda=1;//to avoid divide 0 and the system completely stop work
    	}
    	if(DEBUG){
    	printf("Update lambda %d to %d\n",prev_lambda,lambda);
    	fflush(stdout);}
    	lambda_log[lambda_count][1]=lambda;
    	struct timeval tv;
		gettimeofday(&tv, NULL);
		lambda_log[lambda_count][0]=tv.tv_sec;
    	lambda_count+=1;


	}

}


int main( int argc, char *argv[] )
{	/* Validate the input format */
	if ( argc != 6 )
	{
		printf( "Insufficient number of args. Correct Format: ./streamerd tcp-port payload-size init-lambda mode logfile1\n" );
		return(-1);
	}
	int socket_fd, connect_fd;
	//build tcp socket based on tcp-port
	struct sockaddr_in	addr1;
	memset( &addr1, 0, sizeof(addr1) );
	addr1.sin_family	= AF_INET;
	addr1.sin_port		= htons( atoi( argv[1] ) );
	char *ip_now = getip();
	//printf("Local Ip address %s\n",ip_now);
	addr1.sin_addr.s_addr =inet_addr(ip_now);
	//init socket
	if ( (socket_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
	{
		printf( "create socket error:%s(errno :%d)\n", strerror( errno ), errno );
		return(-1);
	}
	if ( bind( socket_fd, (struct sockaddr *) &addr1, sizeof(addr1) ) == -1 )
	{
		printf( "bind socket error:%s(errno:%d)\n", strerror( errno ), errno );
		return(-1);
	}
	if ( listen( socket_fd, MAX_LISTEN ) == -1 )
	{
		printf( "listen socket error:%s(errno:%d)\n", strerror( errno ), errno );
		return(-1);
	}

	struct sockaddr_in	listendAddr;
	int	listendAddrLen = sizeof(listendAddr);
	getsockname( socket_fd, (struct sockaddr *) &listendAddr, &listendAddrLen ); /* get the port */
	printf( "listen address = %s:%d\n", inet_ntoa( listendAddr.sin_addr ), ntohs( listendAddr.sin_port ) );
	struct sockaddr_in	client_addr;
	int			client_addrLen = sizeof(client_addr);
	char buf[MAXLEN];
	int block_size=atoi(argv[2]);
	payload_size=block_size;
	control_mode=atoi(argv[4]);
	lambda=atoi(argv[3]);//influx rate
	char *logfile1=argv[5];
	//init all the method's dependency factor
	FILE *fp1;
	fp1=fopen("control-param.dat","r");
	if(fp1==NULL)
	{
		printf("can not load file!");
		return 1;
	}
	char line[1000];
	int count1=0;
	while(!feof(fp1))
	{
		fgets(line,1000,fp1);
		count1+=1;
		//printf("%d %s",count1,line);
		//fflush(stdout);

		if(count1==1){
			a_factor=atof(line);
		}
		else if(count1==2){
			delta_factor=atof(line);
		}else if(count1==3){
			epsilon_factor=atof(line);
		}else if(count1==4){
			beta_factor=atof(line);
		}

	}
	close(fp1);
	printf("Get information: alpha %.4f, delta %.4f, epsilon_factor %.4f, beta_factor %.4f\n",a_factor,delta_factor,epsilon_factor,beta_factor);
	fflush(stdout);
	int k;
	int len;
	//process request
	while(1){
		if ( (connect_fd = accept( socket_fd, (struct sockaddr *) &client_addr, &client_addrLen ) ) == -1 )
		{
			printf( "accept socket error :%s(errno:%d)\n", strerror( errno ), errno );
			continue;
		}
		k = fork();
  		if (k==0) {
  			//process the accept info
  			len		= recv( connect_fd, buf, MAXLEN, 0 ); /* read can only be used for linux, therefore, i change to recv */
			buf[len]	= '\0';
			char final_path[200];
			//final_path[0]='\0';
			strcpy (final_path,"/tmp/");
			strcat(final_path,buf);
			printf("Access final path of requesting file:%s\n", final_path);
			//char *return_label;
        	//int execute_label=0;
        	char block_data[block_size];//init data for each block
			if ( access(final_path,0) ){
				printf("Requesting file does not exist!");
				char ACK[1000];
				ACK[0]='0';
				ACK[1]='\0';
				write(connect_fd,ACK,strlen(ACK));
				exit(1);
			}else if(calculate_file_size(final_path)==0){
				printf("Requesting file includes nothing!");
				char ACK[1000];
				ACK[0]='0';
				ACK[1]='\0';
				write(connect_fd,ACK,strlen(ACK));
				exit(1);
			}else{
				//init a udp socket
				if ( (udp_socket = socket( AF_INET, SOCK_DGRAM, 0 ) ) == -1 )
				{
					printf( "create socket error:%s(errno :%d)\n", strerror( errno ), errno );
					exit(1);
				}

				struct sockaddr_in servaddr;
				memset( &servaddr, 0, sizeof(servaddr) );
				servaddr.sin_family	= PF_INET;
				servaddr.sin_port	= 0; /* configure port, this means automatically assigned by the linux */
				/* configure ip */
				char *ip_now1 = getip();
				servaddr.sin_addr.s_addr =inet_addr(ip_now1);
				if (bind( udp_socket, (struct sockaddr *) &servaddr, sizeof(servaddr) ) == -1 )
				{
					printf( "bind socket error:%s(errno:%d)\n", strerror( errno ), errno );
					exit(1);
				}
				//get the port number
				struct sockaddr_in	listendAddr1;
				int	listendAddrLen1 = sizeof(listendAddr1);
				getsockname( udp_socket, (struct sockaddr *) &listendAddr1, &listendAddrLen1 ); /* get the port */
				printf( "Current udp address = %s:%d\n", inet_ntoa( listendAddr1.sin_addr ), ntohs( listendAddr1.sin_port ) );
				fflush(stdout);
				//prepare send back info
				char ACK[1000];
				ACK[0]='2';
				//ACK[1]=',';
				ACK[1]='\0';
				char tmp_listen_port[100];
				sprintf(tmp_listen_port, " %d" , ntohs( listendAddr1.sin_port ) );
				strcat(ACK,tmp_listen_port);
				//strcat(ACK,",");
				//finally append the file size
				struct stat statbuf;
    			stat(final_path,&statbuf);
    			int file_size=statbuf.st_size;
    			char file_size_str[100];
    			sprintf(file_size_str, " %d" , file_size);
    			strcat(ACK,file_size_str);
    			printf("Notification info to client:%s\n",ACK);
    			fflush(stdout);
    			//send this by tcp
    			signal(SIGIO,update_lambda);
    			fcntl(udp_socket,F_SETOWN,getpid());
    			int flags=fcntl(udp_socket,F_GETFL);
    			if(flags<0 || fcntl(udp_socket,F_SETFL,flags|O_ASYNC) <0)
   				{
       				/*setting asyny mode*/
       				printf("fcntl error\n");
       				fflush(stdout);
   				}else{
					printf("fcntl successful\n");
					fflush(stdout);
   				}
    			write(connect_fd,ACK,strlen(ACK));

				// first use tcp to transfer back the new port
    			//request a global label to continue really writing
    			printf("Coming to data transferring stage\n");
    			fflush(stdout);

				FILE *fp = fopen(final_path, "r");
        		if(NULL == fp)
        		{
            		printf("File:%s Not Found\n", final_path);
            		exit(1);
        		}
        		//register sigio to process the information to update lambda
        		//open a new udp port
        		float current_sleep=1/lambda;
        		int transfer_size;
        		bzero(block_data,block_size);
        		int read_length;
        		char real_block_data[block_size+4];
        		real_block_data[0]='\0';
        		struct timespec req;
        		struct timespec time_buf;
        		int second_sleep=0;
        		int sleep_state=0;
        		while(1){
        			if(transfer_label){
        				int sequence_number=0;
        				while((read_length = fread(block_data, sizeof(char),block_size, fp)) > 0)
            			{
            				//block_data[read_length]='\0';
            				sprintf(real_block_data, "%4d" , sequence_number);
            				real_block_data[4]='\0';
            				strcat(real_block_data,block_data);
            				//printf("%s\n",real_block_data);
            				//exit(1);
            				//nanosleep(current_sleep);
            				current_sleep=1/(float)lambda;
            				//second_sleep=(int)(current_sleep/1000);
            				//req.tv_sec=0;
            				//req.tv_nsec=(int)((current_sleep)*1000000000);
            				//time_buf=req;
            				// sleep_state=nanosleep(&req,&time_buf);
            				// if(sleep_state==-1){
            				// 	printf("nano sleep calling is not successful with %d nano second!!!\n",req.tv_nsec);
            				// 	fflush(stdout);
            				// }
            				lambda_log[lambda_count][1]=lambda;
    						struct timeval tv;
							gettimeofday(&tv, NULL);
							lambda_log[lambda_count][0]=tv.tv_sec;
    						lambda_count+=1;
            				mssleep((int)(current_sleep*1000*1000*1000));
                			sendto(udp_socket, real_block_data, strlen(real_block_data), 0, (struct sockaddr *) &send_to_addr, sizeof(send_to_addr) );
                			bzero(block_data, block_size);
                			sequence_number+=1;
           	 			}
           	 			break;

        			}

        		}
        		ACK[0]='5';
        		ACK[1]='E';
        		ACK[2]='N';
        		ACK[3]='D';
        		ACK[4]='\0';
        		// transmit 5 times termination
        		for(int kk=0;kk<5;kk++){
					write(connect_fd,ACK,strlen(ACK));
        		}
        		close(fp);
        		//write all the lambda record to the logfile1
        		FILE *fpWrite=fopen(logfile1,"w");
        		if(fpWrite==NULL)
        			{   printf("Writing lambda record failed\n");
        				fflush(stdout);
        				return 0;
        			}
        		for(int i=0;i<lambda_count;i++)
        			{fprintf(fpWrite,"%d,%d\n",lambda_log[i][0],lambda_log[i][1]); }
        		fclose(fpWrite);
        		printf("This sender part completely finished\n");
        		printf("-------------------------------------------------");
        		fflush(stdout);
			}
			close(connect_fd);
  		}
  		close(connect_fd);
	}



}