#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
void mssleep(int const time_in_ms)
{   
    struct timespec time;
    struct timespec time_buf;
 
    int ret = -1;
    time.tv_sec = (time_in_ms / 1000);
    time.tv_nsec = (1000 * 1000 * (time_in_ms % 1000));
    printf("Seconds %d,nano %d",time.tv_sec,time.tv_nsec);
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
 
int main()
{
	time_t t;
    struct tm * lt;
    struct timeval tv;
 
	for(int i=0; i<4; i++)
	{
		gettimeofday(&tv, NULL);
		printf("millisecond:%ld\n\n",tv.tv_sec*1000 + tv.tv_usec/1000);  //毫秒
		//printf("microsecond:%ld\n",tv.tv_sec*1000000 + tv.tv_usec);  //微秒
		mssleep(100);
	}
	
	
  
	return 0;
}
