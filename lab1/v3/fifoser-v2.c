#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>


int main()
{
	int fd, fd1, count;
    pid_t k;
    int status;
	// FIFO file path
	char *myfifo = "server_queue";
	char *myfifo1 = "client_queue";
    char *buf[10], *pch;
	char arr1[80];

	mkfifo(myfifo, 0666);

	while (1)
	{
	    //set initial values
	    count = 0;
	    memset(buf, '\0', sizeof(char*) * 10);
	    memset(arr1, '\0', sizeof(char) * 80);

	    //open FIFO to read and close it
        fd = open(myfifo, O_RDONLY);
		read(fd, arr1, sizeof(arr1));
        close(fd);

        //parse commands
        pch = strtok(arr1," ");
        while(pch != NULL){
            buf[count] = pch;
            pch = strtok(NULL," ");
            count++;
        }

        //execute without waiting
        k = fork();
        if(k==0){
            //write output to fd1 instead of stdout
            fd1 = open(myfifo1, O_WRONLY);
            dup2(fd1, STDOUT_FILENO);
            close(fd1);
            if(execvp(buf[0],buf) == -1)exit(1);
        }
	}
	unlink(myfifo);
	return 0;
}

