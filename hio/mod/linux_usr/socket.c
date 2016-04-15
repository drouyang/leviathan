/* socket Test Process 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
SYSCALL_DEFINE4(send, int, fd, void __user *, buff, size_t, len,
                unsigned int, flags)
{
        return sys_sendto(fd, buff, len, flags, NULL, 0);
}
SYSCALL_DEFINE4(recv, int, fd, void __user *, ubuf, size_t, size,
                unsigned int, flags)
{
        return sys_recvfrom(fd, ubuf, size, flags, NULL, NULL);
}
*/

int main(int argc, char* argv[])
{

    return 0;
    int ret;
    char sendBuff[1025];
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr; 
    int port = 5000;

    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    ret = socket(AF_INET, SOCK_STREAM, 0);
    printf( "socket returns %d\n", ret);
    if (ret < 0) return -1;


    listenfd = ret;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port); 

    ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 
    printf("bind returns %d\n", ret);
    if (ret < 0) return -1;

    ret = listen(listenfd, 10); 
    printf("listen returns %d\n", ret);
    if (ret < 0) return -1;

    ret = accept(listenfd, (struct sockaddr*)NULL, NULL); 
    printf("accept returns %d\n", ret);
    if (ret < 0) return -1;

    connfd = ret;
    sprintf(sendBuff, "Hello World!\n");
    send(connfd, sendBuff, strlen(sendBuff), 0); 


    return 0;
}
