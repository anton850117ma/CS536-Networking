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
#include <alsa/asoundlib.h>
#include <malloc.h>
#include "debug.h"
#define MAXLEN		65538 /* Max Command execution output characters to be written */
#define MAX_LISTEN	10

int udp_socket;
struct sockaddr_in send_to_addr;
int	send_to_addrLen = sizeof(send_to_addr);
static snd_pcm_t *mulawdev;
static snd_pcm_uframes_t mulawfrms;
int BUF_SIZE;
int target_buf;
int queue_start=0;
int queue_end=0;
char *BUF;
int gamma_factor;
int audio_file_size;
int LOG[MAXLEN][2];
int LOG_COUNT=0;
char *logfile2;
#define mulawwrite(x) snd_pcm_writei(mulawdev, x, mulawfrms)

void mulawopen(size_t *bufsiz) {
	snd_pcm_hw_params_t *p;
	unsigned int rate = 8000;

	snd_pcm_open(&mulawdev, "default", SND_PCM_STREAM_PLAYBACK, 0);
	snd_pcm_hw_params_alloca(&p);
	snd_pcm_hw_params_any(mulawdev, p);
	snd_pcm_hw_params_set_access(mulawdev, p, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(mulawdev, p, SND_PCM_FORMAT_MU_LAW);
	snd_pcm_hw_params_set_channels(mulawdev, p, 1);
	snd_pcm_hw_params_set_rate_near(mulawdev, p, &rate, 0);
	snd_pcm_hw_params(mulawdev, p);
	snd_pcm_hw_params_get_period_size(p, &mulawfrms, 0);
	*bufsiz = (size_t)mulawfrms;
	return;
}

void mulawclose(void) {
	snd_pcm_drain(mulawdev);
	snd_pcm_close(mulawdev);
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
char buf_chat[MAXLEN];
int buf_chat_len=MAXLEN;
void receive_audio(int signo)  /*deal with the info to update lambda*/
{

	struct sockaddr_in send_to_addr1;
	int	send_to_addrLen1 = sizeof(send_to_addr1);
	bzero(buf_chat,MAXLEN);
    int len= recvfrom(udp_socket, buf_chat, buf_chat_len, 0, &send_to_addr1, &send_to_addrLen1);
    buf_chat[len]='\0';
    //deal with possible termination call
    if(buf_chat[0]=='5'&&buf_chat[1]=='E'&&buf_chat[2]=='N'&&buf_chat[3]=='D'){
    	float accept_rate=queue_end/(float)audio_file_size;
    	printf("Received file %d/%d,receive rate %f",queue_end,audio_file_size,accept_rate);
    	FILE *fpWrite=fopen(logfile2,"w");
        	if(fpWrite==NULL)
        	{   printf("Writing record failed\n");
        				fflush(stdout);
        				exit(1);
        	}
       		for(int i=0;i<LOG_COUNT;i++)
        	{fprintf(fpWrite,"%d,%d\n",LOG[i][0],LOG[i][1]); }
        	fclose(fpWrite);
        exit(1);
    }
    //verify this info comes from the correct info.
    char *current_ip= inet_ntoa(send_to_addr1.sin_addr );
    char *decide_ip=inet_ntoa(send_to_addr.sin_addr );
    if(strcmp(current_ip,decide_ip)!=0){
    	printf("IP of current sender is not correct!!\n");
    	return;
    }
    int current_port=ntohs( send_to_addr1.sin_port );
    int decide_port=ntohs( send_to_addr.sin_port );
    if(decide_port!=current_port){
    	printf("Port of current sender is not correct!!\n");
    	return;
    }
    for(int k=4;k<len;k++){
    	BUF[queue_end%BUF_SIZE]=buf_chat[k];
    	queue_end+=1;
    	if(queue_end-queue_start>BUF_SIZE){
    		LOG[LOG_COUNT][1]=queue_end-queue_start;
    		struct timeval tv;
			gettimeofday(&tv, NULL);
			LOG[LOG_COUNT][0]=tv.tv_sec;
			LOG_COUNT+=1;
    		printf("In the receiving process, the buffer is not enough to hold the data now\n");
    		return;
    	}
    }
    //give feed backs to the other udp address
    char ACK[1000];
    ACK[0]='\0';
    char tmp_str[20];
    sprintf(tmp_str,"%d ",queue_end-queue_start);
    strcat(ACK,tmp_str);
    tmp_str[0]='\0';
    sprintf(tmp_str,"%d ",target_buf);
    strcat(ACK,tmp_str);
    tmp_str[0]='\0';
    sprintf(tmp_str,"%d ",gamma_factor);
    strcat(ACK,tmp_str);
    if(DEBUG){
    printf("Sending back current info to sender:%s total received:%d/%d\n",ACK,queue_end,audio_file_size);
    fflush(stdout);}
    sendto(udp_socket, ACK, strlen(ACK), 0, (struct sockaddr *) &send_to_addr, sizeof(send_to_addr) );
    LOG[LOG_COUNT][1]=queue_end-queue_start;
    struct timeval tv;
	gettimeofday(&tv, NULL);
	LOG[LOG_COUNT][0]=tv.tv_sec;
	LOG_COUNT+=1;
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
int main( int argc, char *argv[] )
{	/* Validate the input format */
	if ( argc != 9 )
	{
		printf( "Insufficient number of args. Correct Format: ./playaudio tcp-ip tcp-port audiofile payload-size gamma buf-size target-buf logfile2\n" );
		return(-1);
	}
	//init local tcp
	int socket_fd;
	struct sockaddr_in	addr1;
	memset( &addr1, 0, sizeof(addr1) );
	addr1.sin_family	= AF_INET;
	addr1.sin_port		= htons( atoi( argv[2] ) );
	//printf("Local Ip address %s\n",ip_now);
	addr1.sin_addr.s_addr =inet_addr(argv[1]);
	if ( (socket_fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 )
	{
		printf( "create socket error:%s(errno :%d)\n", strerror( errno ), errno );
		return(-1);
	}
	//connect to the tcp address
	if(connect(socket_fd,(struct sockaddr *)&addr1,sizeof(struct sockaddr))<0)
    {
        perror("connect to the server tcp failed\n");
        return 1;
    }
    char buf[100];
    int buf_size=100;
    char *audiofile=argv[3];
    send(socket_fd,audiofile,strlen(audiofile),0);
    //receive info from the server to get server udp port
     int len=recv(socket_fd,buf,buf_size,0);
     buf[len]='\0';
     if(buf[0]!='2'){
     	printf("The server give back incorrect information, terminate!!!\n");
     	exit(0);
     }
     int extract_size;
     char **connect_info= explode(' ', buf, &extract_size);
     if(extract_size!=3){
     	printf("The server give back incorrect format, should be 2 [udp_port], terminate!!!\n");
     	exit(0);
     }
     int send_udp_port=atoi(connect_info[1]);
     audio_file_size=atoi(connect_info[2]);
     //then binds to own udp
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
		printf( "bind udp socket error:%s(errno:%d)\n", strerror( errno ), errno );
		exit(1);
	}
	//get client port
	struct sockaddr_in	listendAddr1;
	int	listendAddrLen1 = sizeof(listendAddr1);
	getsockname( udp_socket, (struct sockaddr *) &listendAddr1, &listendAddrLen1 ); /* get the port */
	printf( "own udp service address = %s:%d\n", inet_ntoa( listendAddr1.sin_addr ), ntohs( listendAddr1.sin_port ) );
	int client_udp_port=ntohs( listendAddr1.sin_port );
	//make udp send to address
	memset( &send_to_addr, 0, sizeof(send_to_addr) );
	send_to_addr.sin_family	= PF_INET;
	send_to_addr.sin_port	= htons(send_udp_port); /* configure port, this means automatically assigned by the linux */
	/* configure ip */
	send_to_addr.sin_addr.s_addr =inet_addr(argv[1]);
	printf( "Sender udp service address = %s:%d\n", inet_ntoa( send_to_addr.sin_addr ), ntohs( send_to_addr.sin_port ) );
	fflush(stdout);
	//send own udp port
	bzero(buf,100);
	strcat(buf,"99 ");
	char self_port_str[100];
	sprintf(self_port_str, "%d" , client_udp_port);
	strcat(buf,self_port_str);
	//send to server
	printf("sending udp confirmation socket to the sender:%s\n",buf);
	fflush(stdout);
	sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr *) &send_to_addr, sizeof(send_to_addr) );

	int play_block_size=atoi(argv[4]);//use to play audios
	gamma_factor=atoi(argv[5]);
	BUF_SIZE=atoi(argv[6]);
	target_buf=atoi(argv[7]);
	logfile2=argv[8];

	int kkk;
	kkk = fork();

  	if (kkk==0) {

		BUF= (char *)malloc(BUF_SIZE*sizeof(char));
		signal(SIGIO,receive_audio);
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

		size_t audio_size;
		mulawopen(&audio_size);
		//char audio_buf[audio_size];		// audio buffer: holds one block
		char *audio_buf;
		audio_buf= (char *)malloc(audio_size);
		int prefead_label=0;
		struct timespec req;
		struct timespec time_buf;
		int sleep_state=0;
		char *audio_play_buffer;
	//use the
	//play the audio
		int audio_count=0;
		while(1){
			if(queue_end-queue_start>target_buf&&prefead_label==0){
				printf("Prefetch has reached Q* requirment.\n");
				fflush(stdout);
				prefead_label=1;
			}
			if(queue_end-queue_start>play_block_size&&prefead_label){
				//if this can fill a play block :play+sleep
				for(int k=0;k<play_block_size;k++){
					audio_buf[k]=BUF[queue_start%BUF_SIZE];
					queue_start+=1;
				}
				//printf("%s %d\n",audio_buf,strlen(audio_buf));
				//exit(0);
				/*audio_play_buffer = (char *)malloc(audio_size);
				audio_count=0;
				while(audio_count<play_block_size){
					for(int k=0;k<audio_size;k++){
						if(audio_count<play_block_size){
							audio_play_buffer[k]=audio_buf[audio_count];
							audio_count+=1;
						}
						if(audio_count>=play_block_size){
							audio_play_buffer[k]='\0';
						}
					}
				}*/
				mulawwrite(audio_buf);
				req.tv_sec=0;
        		req.tv_nsec=(int)((1/(float)gamma_factor)*1000000000);
        		LOG[LOG_COUNT][1]=queue_end-queue_start;
        		if(DEBUG){
        		printf("Current buffer taken %d, now played %d\n,sleeped %d nanoseconds\n",queue_end-queue_start,play_block_size,req.tv_nsec);
        		fflush(stdout);}
    			struct timeval tv;
				gettimeofday(&tv, NULL);
				LOG[LOG_COUNT][0]=tv.tv_sec;
				LOG_COUNT+=1;
				// time_buf=req;
				// sleep_state=nanosleep(&req,&time_buf);
    //     		if(sleep_state==-1){
    //         		printf("nano sleep calling is not successful with %d nano second!!!\n",req.tv_nsec);
    //         		fflush(stdout);
    //     		}
				mssleep((int)((1/(float)gamma_factor)*1000*1000*1000));
        	}
		}

		mulawclose();
	}

	//block here to wait for the temination command
	while(1){
		//printf("Control Pannel:Coming to waiting tcp final termination stage\n");
		//fflush(stdout);
		len=read(socket_fd,buf,buf_size);
     	buf[len]='\0';
     	//printf("%s %d",buf,len);
     	//fflush(stdout);
     	if(buf[0]=='5'&&buf[1]=='E'&&buf[2]=='N'&&buf[3]=='D'){
     		//finally termination.
     		//to call the output, send a info to our own udp to let it sleep and output the info.
     		printf("Received termination info:%s\n",buf);
     		fflush(stdout);

     		//send this to own udp port
     		sendto(udp_socket, buf, strlen(buf), 0, (struct sockaddr *) &listendAddr1, sizeof(listendAddr1) );
     		break;
     	}


	}





}