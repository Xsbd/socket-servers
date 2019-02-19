#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"

#define PORT "9090"

int main(void)
{
	int sockfd = listen_inet(PORT);

	struct sockaddr_storage their_addr;
	socklen_t sin_size = sizeof their_addr;

	while(1) { /* server keeps on running */
		int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
	        if (new_fd < 0) {
			perror_die("accept");
	        }

		connection_report((struct sockaddr *)&their_addr, sin_size);
	
		if (send(new_fd, "Hello, servers!", 13, 0) == -1)
			perror("send");

		close(new_fd);
	}

	close(sockfd);

	return 0;
}
