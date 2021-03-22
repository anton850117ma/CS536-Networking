#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int compare(char *str1, char *str2){

    int result = 1;
    char* new_string1 = (char*)malloc(30*sizeof(char));
    char* new_string2 = (char*)malloc(30*sizeof(char));

    strncpy(new_string1,str1,30);
    strncpy(new_string2,str2,30);
    
    if(strcmp(new_string1,new_string2)==0) result = 0;
    
    free(new_string1);
    free(new_string2);
    return result;
}

char **parse(char *buffer){

    int count = 0;
    char *pch;
    char **command = (char**)malloc(30*sizeof(char*));

    memset(command, '\0', sizeof(char*) * 30);

	pch = strtok(buffer," ");
    while(pch != NULL){
     	command[count] = pch;
      	pch = strtok(NULL," ");
     	count++;
    }
    return command;
}

void free_char(char** commands){
    for(int i = 0; i < 30; ++i){
        free(commands[i]);
    }
    // free(commands);
}

char myencode(char byte, char *prikey, int turn){

    unsigned int key = (unsigned int)strtoul(prikey, NULL, 0);
    unsigned int mask = 0xFF;
    unsigned int subkey = key & mask << turn * 2;
    return byte ^ subkey;
}

char mydecode(char byte, char *pubkey, int turn){

    unsigned int key = (unsigned int)strtoul(pubkey, NULL, 0);
    unsigned int mask = 0xFF;
    unsigned int subkey = key & mask << turn * 2;
    return byte ^ subkey;
}



