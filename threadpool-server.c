#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"
#include "threadpool.h"

typedef struct { int sockfd; } tconf_t;
/* Server states */
typedef enum { WAIT_FOR_MSG, IN_MSG } ServerState;

void serve_connection(int sockfd) {
/* serves connected client */

	/* assume well behaved clients that waits for server hello */
	if (send(sockfd, "*", 1, 0) < 1)
	{printf("%d", sockfd);
		perror_die("server: send");}

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

void server_thread(void *arg) {
/* server threads to server_connection to multiple clients at the same time */
	tconf_t* data = (tconf_t*)arg;
	int sockfd = data->sockfd;
	free(data);
	
	unsigned long id = (unsigned long)pthread_self();
	printf("Thread %lu created to handle connection with socket %d\n", id,
		sockfd);

	serve_connection(sockfd);

	printf("Thread %lu done \n", id);
}

int main(int argc, char** argv)
{
	char *port = "9090";
	if(argc >= 2) {
		port = argv[1];
	}
	int n_threads = 2;
	if(argc >= 3) {
		n_threads = atoi(argv[2]);
	}

	printf("Serving on port: %s\n",port);
	
	tpool_t tp = tpool_create(n_threads);

	int sockfd = listen_inet(port);
	int thr = 0;
	while(1) { /* server keeps on running */
	
		struct sockaddr_storage their_addr;
		socklen_t sin_size = sizeof their_addr;

		int new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (new_fd < 0) {
			perror_die("accept");
		}
		connection_report((struct sockaddr *)&their_addr, sin_size);

		tconf_t* data = (tconf_t*)malloc(sizeof(*data));
		if (!data) {
			die("Thread resource allocation error");
		}
		data->sockfd = new_fd;

		tpool_dispatch(tp, server_thread, data);

	}

	return 0;
}
