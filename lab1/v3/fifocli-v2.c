/*
I found that the last character of the message is '\0' and each line in is followed by a '\n',
meaning that we can just read bytes with read() and print them out as normal. Thus, I just use
a while loop to make sure read() will reach EOF.
*/
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
	int fd1, fd2;
	char str2[80], buf[500];


	// FIFO file path
	char *myfifo = "server_queue";
	char *myfifo1 = "client_queue";
    mkfifo(myfifo1, 0777);

    //open FIFO to write
    fd1 = open(myfifo,O_WRONLY);
    fgets(str2, 80, stdin);
    int len = strlen(str2);
    str2[len-1] = '\0';
    write(fd1, str2, strlen(str2));
    close(fd1);

    memset(buf, '\0', sizeof(char) * 500);
    //read "client_queue" 500 bytes in each loop until reach EOF
    fd2 = open(myfifo1, O_RDONLY);
    while(read(fd2, buf, sizeof(buf))>0){
        printf("%s", buf);
        memset(buf, '\0', sizeof(char) * 500);
    }
    close(fd2);

    unlink(myfifo1);

	return 0;
}
