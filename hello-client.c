#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"

#define PORT "9090"
#define MAXDATASIZE 100 /* max number of bytes we can get at once */

int main(int argc, char *argv[])
{
    int sockfd, numbytes;  
    char buf[MAXDATASIZE];

    if (argc != 2) {
        fprintf(stderr,"usage: hello-client hostname");
	exit(EXIT_FAILURE);
    }
    

    sockfd = connect_inet(argv[1], PORT); 

    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
        perror_die("recv");

    buf[numbytes] = '\0';

    printf("client: received '%s'\n",buf);

    close(sockfd);

    return 0;
}
