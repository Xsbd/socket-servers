/* Multiplexing connection select server
 * accepting mutliple clients concurrently
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockutils.h"

#define MAXFDS 1000
#define SENDBUF_SIZE 1024

typedef enum { INITIAL_ACK, WAIT_FOR_MSG, IN_MSG } ServerState;

typedef struct {
	ServerState state;

	/* sendbuf is the server buffer,
	 * on_peer_ready_recv handler populates it,
	 * on_peer_ready_sned handler drains it
	 */
	uint8_t sendbuf[SENDBUF_SIZE];
	/* sendbuf_end points to last valid byte in sendbuf */
	int sendbuf_end;
	/* sendptr is the next byte to send */
	int sendptr;
} peer_state_t;

/* fd is global. i.e Each peer has a unique fd in global scope,
 * when a peer disconnects fd is released to be used by another
 * on_peer_connected should initialize the state properly to remove
 * trace of the old peer on the same fd
 */
peer_state_t global_state[MAXFDS];
 
/* the return structure of callback functions
 * tell if the port should be kept monitoring for read/write
 */
typedef struct {
	bool want_read;		/* true -> keep monitoring fd for reading */
	bool want_write;	/* true -> keep monitoring fd for writing */
} fd_status_t;

/* these constants make creating fd_status_t values less verebose */
const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};

fd_status_t on_peer_connected(int sockfd, const struct sockaddr* peer_addr,
				socklen_t peer_addr_len) {
	assert(sockfd < MAXFDS);
	connection_report(peer_addr, peer_addr_len);

	// Initialize state to send back a '*' to the peer immediately.
	peer_state_t* peerstate = &global_state[sockfd];
	peerstate->state = INITIAL_ACK;
	peerstate->sendbuf[0] = '*';
	peerstate->sendptr = 0;
	peerstate->sendbuf_end = 1;

	// Signal that this socket is ready for writing now.
	return fd_status_W;
}

fd_status_t on_peer_ready_recv(int sockfd) {
	assert(sockfd < MAXFDS);
	peer_state_t* peerstate = &global_state[sockfd];

	if (peerstate->state == INITIAL_ACK || peerstate->sendptr < peerstate->sendbuf_end) {
		/* Initial ack sending not complete or nothing to send */
		return fd_status_W;
	}

	uint8_t buf[1024];
	int nbytes = recv(sockfd, buf, sizeof buf, 0);
	if (nbytes == 0) {
		/* assume peer disconnected */
		return fd_status_NORW;
	} else if (nbytes < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			/* socket is not really ready to receive */
			return fd_status_R;
		} else {
			perror_die("recv");
		}
	}
	bool ready_to_send = false;
	for (int i=0; i<nbytes; ++i) {
		switch (peerstate->state) {
			case INITIAL_ACK:
				assert(0 && "can't reach here");
				break;
			case WAIT_FOR_MSG:
				if (buf[i] == '^') {
					peerstate->state = IN_MSG;
				}
				break;
			case IN_MSG:
				if (buf[i] == '$') {
					peerstate->state = WAIT_FOR_MSG;
				} else {
					assert(peerstate->sendbuf_end < SENDBUF_SIZE);
					peerstate->sendbuf[peerstate->sendbuf_end++] = buf[i] +1;
					ready_to_send = true;
				}
				break;
		}
	}
	/* Report reading readiness iff there's nothing to send to the peer as
	 * a result of the latest recv
	 */
	return (fd_status_t){.want_read = !ready_to_send,
				.want_write = ready_to_send};
}

fd_status_t on_peer_ready_send(int sockfd) {
	assert(sockfd < MAXFDS);
	peer_state_t* peerstate = &global_state[sockfd];

	if (peerstate->sendptr >= peerstate->sendbuf_end) {
		/* Nothing to send */
		return fd_status_RW;
	}
	int sendlen = peerstate->sendbuf_end - peerstate->sendptr;
	int nsent = send(sockfd, &peerstate->sendbuf[peerstate->sendptr], sendlen, 0);
	if (nsent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return fd_status_W;
		} else {
			perror_die("send");
		}
	}
	if (nsent < sendlen) {
		peerstate->sendptr += nsent;
		return fd_status_W;
	} else {
		/* Everythong was sent successfully; reset the queue */
		peerstate->sendptr = 0;
		peerstate->sendbuf_end = 0;

		/* Special-case state transition in if we were in INITIAL_ACK until now */
		if (peerstate->state == INITIAL_ACK) {
			peerstate->state = WAIT_FOR_MSG;
		}

		return fd_status_R;
	}
}

int main (int argc, char* argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);

	char *port = "9090";
	if (argc >= 2) {
		port = argv[1];
	}
	printf("Serving on port %s\n", port);

	int listener_sockfd = listen_inet(port);

	/* select() can return a read notification for a socket 
	 * that isn't actually readable
	 * use non blocking socket
	 */
	make_socket_non_blocking(listener_sockfd);

	if (listener_sockfd >= FD_SETSIZE) {
		die("listener socket fd (%d) >= FD_SETSIZE (%d)", listener_sockfd, FD_SETSIZE);
	}

	/* track all the FDs in main */
	fd_set readfds_master;
	FD_ZERO(&readfds_master);
	fd_set writefds_master;
	FD_ZERO(&writefds_master);

	/* use the readfds to track all incoming connections */
	FD_SET(listener_sockfd, &readfds_master);

	/* track the maximal FD seen so that select doesn't need to iterate to FD_SETSIZE */
	int fdset_max = listener_sockfd;

	while (1) {
		/* pass copies of fd_sets, since select() modifies passed values */
		fd_set readfds = readfds_master;
		fd_set writefds = writefds_master;

		int nready = select(fdset_max +1, &readfds, &writefds, NULL, NULL);
		if (nready < 0) {
			perror_die("select");
		}

		for (int fd = 0; fd <= fdset_max && nready > 0; fd++) {
			/* Check if this fd became readable */
			if (FD_ISSET(fd, &readfds)) {
				nready--;

				if (fd == listener_sockfd) {
					/* new peer connected */
					struct sockaddr_storage peer_addr;
					socklen_t peer_addr_len = sizeof(peer_addr);
					int newsockfd = accept(listener_sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len);
					if (newsockfd < 0) {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							printf("accept returned EAGAIN or EWOULDBLOCK\n");
						} else {
							perror_die("accept");
						}
					} else {
						make_socket_non_blocking(newsockfd);
						if (newsockfd > fdset_max) {
							if (newsockfd >= FD_SETSIZE) {
								die("socket fd (%d) >= FD_SETSIZE (%d)", newsockfd, FD_SETSIZE);
							}
							fdset_max = newsockfd;
						}

						fd_status_t status = on_peer_connected(newsockfd, (struct sockaddr*)&peer_addr, peer_addr_len);
						if (status.want_read) {
							FD_SET(newsockfd, &readfds_master);
						} else {
							FD_CLR(newsockfd, &readfds_master);
						}
						if (status.want_write) {
							FD_SET(newsockfd, &writefds_master);
						} else {
							FD_CLR(newsockfd, &writefds_master);
						}
					}
				} else {
					fd_status_t status = on_peer_ready_recv(fd);
					if (status.want_read) {
						FD_SET(fd, &readfds_master);
					} else {
						FD_CLR(fd, &readfds_master);
					}
					if (status.want_write) {
						FD_SET(fd, &writefds_master);
					} else {
						FD_CLR(fd, &writefds_master);
					}
					if (!status.want_read && !status.want_write) {
						printf("socket %d closing\n", fd);
						close(fd);
					}
				}
			}

			/* Check if this fd became writable */
			if (FD_ISSET(fd, &writefds)) {
				nready--;
				fd_status_t status = on_peer_ready_send(fd);
				if (status.want_read) {
					FD_SET(fd, &readfds_master);
				} else {
					FD_CLR(fd, &readfds_master);
				}
				if (status.want_write) {
					FD_SET(fd, &writefds_master);
				} else {
					FD_CLR(fd, &writefds_master);
				}
				if (!status.want_read && !status.want_write) {
					printf("socket %d closing\n", fd);
					close(fd);
				}
			}
		}
	}

	return 0;
}
