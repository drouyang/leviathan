/* socket Test Process 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

int main(int argc, char* argv[])
{
    int ret;

    //int port = 80;

    ret = socket( PF_INET, SOCK_STREAM, 0 );
    printf( "socket() returns %d\n", ret);

    return 0;
}
