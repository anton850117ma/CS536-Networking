#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <string.h>

void first_cmd(int argc, char **argv);

//function for handle command-line arguments
void first_cmd(int argc, char **argv){

    pid_t k;
    int status;
    char* buf[100];
    int count = 0;

    memset(buf, '\0', sizeof(char*) * 100);

    //check if there is a command-line argument
    if(argc == 2){

        //split the commands into tokens with whitespace
        char *pch;
        pch = strtok(argv[1]," ");
        while (pch != NULL)
        {
            buf[count] = pch;
            pch = strtok(NULL," ");
            count++;
        }

        //execute
        k = fork();
        if (k==0) {
            if(execvp(buf[0],buf) == -1)exit(1);
        }
        else {
            waitpid(k, &status, 0);
        }
	}
}

int main(int argc, char **argv)
{
    pid_t k;
    char buf[100];
    int status;
    int len;

    //deal with command-line arguments
    first_cmd(argc, argv);

    //below is the same as simsh.c

    while(1) {

        // print prompt
        fprintf(stdout,"[%d]$ ",getpid());

        // read command from stdin
        fgets(buf, 100, stdin);

        len = strlen(buf);
        if(len == 1)				// only return key pressed
            continue;

        buf[len-1] = '\0';

        k = fork();
        if (k==0) {
        // child code
            if(execlp(buf,buf,NULL) == -1)	// if execution failed, terminate child
                exit(1);
        }
        else {
        // parent code
            waitpid(k, &status, 0);
        }
    }
}
