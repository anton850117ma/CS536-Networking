#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>

//global variables
char *myfifo = "server_queue";
char *myfifo1 = "client_queue";
char str2[80], buf[500];
int fd1, fd2, flag = 1;
void retxreq(int sig);

void retxreq(int sig){

    printf("%d try failed!\n\n", flag++);
    //if flag is not 4, resent the request
    if(flag < 4){
        alarm(2);
        fd1 = open(myfifo,O_WRONLY);
        write(fd1, str2, strlen(str2));
        close(fd1);
    }else{
        //if forth try also fails, terminate
        printf("I give up!\n");
	unlink(myfifo1);
        exit(sig);
    }
}

int main(){

	// FIFO file path
    mkfifo(myfifo1, 0777);
    signal(SIGALRM, retxreq);

    //open FIFO to write
    fd1 = open(myfifo,O_WRONLY);
    fgets(str2, 80, stdin);
    int len = strlen(str2);
    str2[len-1] = '\0';
    memset(buf, '\0', sizeof(char) * 500);

    //start the clock and write the request
    alarm(2);
    write(fd1, str2, strlen(str2));
    close(fd1);

    //read "client_queue" 500 bytes in each loop until reach EOF
    fd2 = open(myfifo1, O_RDONLY);
    while(read(fd2, buf, sizeof(buf))>0){
        //reset alarm() because of receiving the response from the server
        alarm(0);
        printf("%s", buf);
        memset(buf, '\0', sizeof(char) * 500);
    }
    close(fd2);

    unlink(myfifo1);

	return 0;
}
