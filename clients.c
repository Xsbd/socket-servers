#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "sockutils.h"

#define MAXDATASIZE 1024 /* max number of bytes we can get at once */

/* Structure for arguments to pass to client_thread() */
typedef struct _thread_data_t {
	int id, sockfd;
	char *host, *port;
	char *msg[3];
	char buf[MAXDATASIZE];
} thread_data_t;

void *client_send(void *arg) {
/* Sending operation thread */
	thread_data_t *data = (thread_data_t *)arg;
	for (int i=0; i<3; i++) {
		printf("conn%d sending %s\n", data->id, data->msg[i]);
		if (send(data->sockfd, data->msg[i],
				strlen(data->msg[i]), 0) < 1)
			perror_die("client: send");
		sleep(2);
	}
	pthread_exit(NULL);
}

void *client_recv(void *arg) {
/* Receiving operation thread */
	thread_data_t *data = (thread_data_t *)arg;
	int numbytes, k=0;
	char end[] = "0000";
	while (k != 4) {			/* end sequence check */
		/* receive data */
		if ((numbytes = recv(data->sockfd,
				data->buf, MAXDATASIZE-1, 0)) == -1) {
			perror_die("client: recv");
		}
		/* print the data received */
		printf("conn%d received ", data->id);
		/* find if end sequence has occured */
		int i=0;
		while (i < numbytes) {
			printf("%c", data->buf[i]);
			if (data->buf[i] == '1') {
				end[k++] = '1';
				if (k == 4) break;	/* k is end seq len */
			} else {
				k = 0;
				memset(&(end), 0, sizeof(end));
			}
			i++;
		} 
		printf("\n");
		/* clear recv buffer */
		memset(&(data->buf), 0, sizeof(data->buf));
		sleep(1);
	}
	pthread_exit(NULL);
}

void *client_thread(void *arg) {
/* Individual client thread operations function */
	thread_data_t *data = (thread_data_t *)arg;
	int sockfd = connect_inet(data->host, data->port); 

	int numbytes;  
	char buf[MAXDATASIZE];
	/* wait for server to send ack of connection */
	do {
		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1)
			perror_die("client: recv");
	} while (buf[0] != '*');

	pthread_t sender, receiver;
	data->sockfd = sockfd;

	/* sending thread */
	int rc;
	if ((rc = pthread_create(&sender, NULL, client_send, data))) {
		perror_die("client sender thread creation error");
	}

	/* receiving thread */
	int rv;
	if ((rv = pthread_create(&receiver, NULL, client_recv, data))) {
		perror_die("client receiver thread creation error");
	}

	/* wait for all children to exit */
	pthread_join(sender, NULL);
	pthread_join(receiver, NULL);

	close(sockfd);
	pthread_exit(NULL);
};

int main(int argc, char *argv[])
{
	char *host="localhost", *port="9090";
	int opt, n_clients=1;
	while ((opt = getopt(argc, argv, "n:s:p:")) != -1) {
		switch (opt) {
			case 'n':
				n_clients = atoi(optarg);
				break;
			case 's':
				host = strdup(optarg);
				break;
			case 'p':
				port = strdup(optarg);
				break;
			case '?':
				fprintf(stderr, "usage: clients "
						"[-n number_of_clients] "
						"[-s server] "
						"[-p port_num]\n");
				exit(EXIT_FAILURE);
		}
	}
//	printf("clients=%d host=%s port=%s", n_clients, host, port);

	pthread_t clients[n_clients];		/* client thread objects */

	/* data for threads */
	thread_data_t t_data[n_clients];
	thread_data_t data = { 0, 0, host, port,
		{"^abc$de^abte$f", "xyz^123", "25$^ab0000$abab"},
		""
	};

	time_t start = time(NULL);

	/* create threads */
	for (int i=0; i<n_clients; i++) {
		t_data[i] = data;
		t_data[i].id = i;
		int rc;
		if ((rc = pthread_create(&clients[i], NULL,
					client_thread, &t_data[i]))) {
			perror_die("client thread creation error");
		}
	}

	/* wait for all children to exit */
	for (int i=0; i<n_clients; i++) {
		pthread_join(clients[i], NULL);
	}

	double elapsed = difftime(time(NULL), start);
	printf("Elapsed time: %fs\n", elapsed);

	return EXIT_SUCCESS;
}
