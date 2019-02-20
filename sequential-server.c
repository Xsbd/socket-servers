#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"

#define PORT "9090"

/* Server states */
typedef enum { WAIT_FOR_MSG, IN_MSG } ServerState;

void serve_connection(int sockfd) {
/* serves connected client */

	/* assume well behaved clients that waits for server hello */
	if (send(sockfd, "*", 1, 0) < 1)
		perror_die("server: send");

	ServerState state = WAIT_FOR_MSG;

	while (1) {
		uint8_t buf[1024];
		int len = recv(sockfd, buf, sizeof buf, 0);
		if (len < 0)
			perror_die("server: recv");
		else if (len == 0)
			break;

		for (int i=0; i<len; ++i) {
			switch (state) {
				case WAIT_FOR_MSG:
					if (buf[i] == '^')
						state = IN_MSG;
					break;
				case IN_MSG:
					if (buf[i] == '$')
						state = WAIT_FOR_MSG;
					else {
						buf[i] += 1;
						if (send(sockfd, &buf[i], 1, 0) <1) {
							perror("server: send error");
							close(sockfd);
							return;
						}
					}
					break;
			}
		}
	}

	close(sockfd);
}

int main(void)
{
	int sockfd = listen_inet(PORT);

	while(1) { /* server keeps on running */
		struct sockaddr_storage their_addr;
		socklen_t sin_size = sizeof their_addr;

		int new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
				&sin_size);
	        if (new_fd < 0)
			perror_die("accept");

		connection_report((struct sockaddr *)&their_addr, sin_size);
		serve_connection(new_fd);
		printf("peer done\n");
	}

	return 0;
}
