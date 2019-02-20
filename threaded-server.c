#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"

typedef struct {int sockfd; } thread_conf_t;
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

void *server_thread(void *arg) {
/* server threads to server_connection to multiple clients at the same time */
	thread_conf_t* data = (thread_conf_t*)arg;
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
	printf("Serving on port: %s\n",port);
	
	int sockfd = listen_inet(port);

	while(1) { /* server keeps on running */
		struct sockaddr_storage their_addr;
		socklen_t sin_size = sizeof their_addr;

		int newsockfd = accept(sockfd,
				(struct sockaddr *)&their_addr, &sin_size);
	        if (newsockfd < 0) {
			perror_die("accept");
		}

		connection_report((struct sockaddr *)&their_addr, sin_size);

		/* create a new thread to handle communications,
		 * once connection established */
		pthread_t t_sock;
		thread_conf_t* data = (thread_conf_t*)malloc(sizeof(*data));
		if(!data) {
			die("thread resource allocation error");
		}
		data->sockfd = newsockfd;
		pthread_create(&t_sock, NULL, server_thread, data);

		/* original thread keeps running forever
		 * so, just detach the child threads so that resources from
		 * dead threads willl be cleaned up */
		pthread_detach(t_sock);
	}

	return 0;
}
