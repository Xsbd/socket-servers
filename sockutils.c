/* Utility functions for C internet sockets */
/* Wrapper for C socket API */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* POSIX compliance headers */
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#define _GNU_SOURCE
#include <netdb.h>

#include <arpa/inet.h>
#define N_BACKLOG 64

void die(char* fmt, ...) {
/* Print ERRORs and terminate */
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(EXIT_FAILURE);
}

void* xmalloc(size_t size) {
/* Wrapper for malloc: die if unable to allocate space */
/* -- by default malloc returns NULL */
	void *ptr = malloc(size);
	if (!ptr) {
		die("malloc failed");
	}
	return ptr;
}

void perror_die(char* msg) {
/* Wrapper for perror: extend perror to die */
	perror(msg);
	exit(EXIT_FAILURE);
}

void *get_in_addr(struct sockaddr *sa) {
/* get IPv4 or IPv6 addr */
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int __setup_socket__(char* server, char* port) {
/* Helper function to setup server or client sockets */
	
	int sockfd;
	struct addrinfo hints, *servinfo, *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;		/* Allow IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;	/* Stream socket */
	if (server == NULL) {
		hints.ai_flags = AI_PASSIVE; 		/* use my IP */
	}

	/* get server ip address */
	int rv;
	if ((rv = getaddrinfo(server, port, &hints, &servinfo)) != 0) {
		perror_die((char*)gai_strerror(rv));
	}

	/* loop through all results and bind/connect to the first we can */
	for(p = servinfo; p != NULL; p = p->ai_next) {

		/* create socket fd */
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("can't open socket");
			continue;
		}

		if (server == NULL) {
			/* useful for addr and port reuse at server */
			int opt=1;
			if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
					&opt, sizeof(int)) == -1) {
				perror_die("setsockopt");
			}

			/* bind to port */
			if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				perror("bind");
				continue;
			}
		} else if (server != NULL) {
			/* client connects to server */
			if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
				close(sockfd);
				perror("connect");
				continue;
			}
		}

		break;
	}

	/* check if failed to bind/connect to any */
	if (p == NULL && server == NULL)  {
		fprintf(stderr, "failed to bind");
		exit(EXIT_FAILURE);
	} else if (p == NULL && server) {
		fprintf(stderr, "failed to connect");
		exit(EXIT_FAILURE);
	}

	/* for client print the server ip it connected to */
	if (server != NULL) {
		char s[INET6_ADDRSTRLEN];
		inet_ntop(p->ai_family,
				get_in_addr((struct sockaddr *)p->ai_addr),
				s, sizeof s);
		printf("connected to %s\n", s);
	}

	freeaddrinfo(servinfo);		/* all done with this structure */

	return sockfd;
}

int listen_inet(char* port) {
/* Wrapper for server: socket creation to listen stages */
/* -- uses the provided port number */

	/* setup the socket */
	int sockfd = __setup_socket__(NULL, port);

	/* listen failure */
	if (listen(sockfd, N_BACKLOG) == -1) {
		perror_die("listen");
	}

	return sockfd;
}

int connect_inet(char* server, char* port) {
/* Wrapper for client: socket creation and connection setup stages */

	/* setup and connect */
	return __setup_socket__(server, port);
}

void connection_report(const struct sockaddr* sa, socklen_t salen) {
/* Get info about client that connected */
	char hostbuf[INET6_ADDRSTRLEN];
	char portbuf[NI_MAXSERV];
	if (getnameinfo((struct sockaddr *)sa, salen, hostbuf, INET6_ADDRSTRLEN,
			portbuf, NI_MAXSERV, 0) == 0) {
		/* getnameinfo sets the name in hostbuf, to get the ip: */
	        inet_ntop(sa->sa_family,
			get_in_addr((struct sockaddr *)sa),
			hostbuf, sizeof hostbuf);
		printf("peer (%s, %s) connected\n", hostbuf, portbuf);
	} else {
		printf("peer (unknonwn) connected\n");
	}
}

void make_socket_non_blocking(int sockfd) {
/* make socket non-blocking */
	int flags = fcntl(sockfd, F_GETFL, 0);
	if (flags == -1) {
		perror_die("fcntl F_GETFL");
	}

	if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror_die("fcntl F_SETFL O_NONBLOCK");
	}
}
