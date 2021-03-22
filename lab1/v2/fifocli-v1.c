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
	int fd1;

	// FIFO file path
	char* myfifo = "server_queue";


	char str2[80];

    //open FIFO to write
    fd1 = open(myfifo,O_WRONLY);
    fgets(str2, 80, stdin);
    int len = strlen(str2);
    str2[len-1] = '\0';
    write(fd1, str2, strlen(str2));
    close(fd1);

	return 0;
}

