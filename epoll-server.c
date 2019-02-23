/* Multiplexing connection select server
 * accepting mutliple clients concurrently
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sockutils.h"

#define MAXFDS 16 * 1024
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
	make_socket_non_blocking(listener_sockfd);

	int epollfd = epoll_create1(0);
	if (epollfd < 0) {
		perror_die("epoll_create1");
	}

	struct epoll_event accept_event;
	accept_event.data.fd = listener_sockfd;
	accept_event.events = EPOLLIN;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listener_sockfd, &accept_event) < 0) {
		perror_die("epoll_ctl EPOLL_CTL_ADD");
	}

	struct epoll_event* events = calloc(MAXFDS, sizeof(struct epoll_event));
	if (events == NULL) {
		die("Unable to allocate memory for epoll_events");
	}

	while (1) {
		int nready = epoll_wait(epollfd, events, MAXFDS, -1);
		for (int i = 0; i < nready ; i++) {
			if (events[i].events & EPOLLERR) {
				perror_die("epoll_wait returned EPOLLERR");
			}

			if (events[i].data.fd == listener_sockfd) {
			/* new peer connected */
				struct sockaddr_storage peer_addr;
				socklen_t peer_addr_len = sizeof(peer_addr);
				int newsockfd = accept(listener_sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len);

				if (newsockfd < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
					/* handle nonblocking socket mode */
						printf("accept returned EAGAIN or EWOULDBLOCK\n");
					} else {
						perror_die("accept");
					}
				} else {
					make_socket_non_blocking(newsockfd);
					if (newsockfd >= MAXFDS) {
						die("socket fd (%d) >= MAXFDS (%d)", newsockfd, MAXFDS);
					}

					fd_status_t status = on_peer_connected(newsockfd, (struct sockaddr*)&peer_addr, peer_addr_len);
					struct epoll_event event = {0};
					event.data.fd = newsockfd;
					if (status.want_read) {
						event.events |= EPOLLIN;	
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;	
					}

					if (epoll_ctl(epollfd, EPOLL_CTL_ADD, newsockfd, &event) < 0) {
						perror_die("epoll_ctl EPOLL_CTL_ADD");
					}
				}
			} else {
			// A peer socket is ready.
				if (events[i].events & EPOLLIN) {
				// Ready for reading.
					int fd = events[i].data.fd;
					fd_status_t status = on_peer_ready_recv(fd);
					struct epoll_event event = {0};
					event.data.fd = fd;
					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						printf("socket %d closing\n", fd);
						if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							perror_die("epoll_ctl EPOLL_CTL_DEL");
						}
						close(fd);
					} else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
						perror_die("epoll_ctl EPOLL_CTL_MOD");
					}
				} else if (events[i].events & EPOLLOUT) {
				// Ready for writing.
					int fd = events[i].data.fd;
					fd_status_t status = on_peer_ready_send(fd);
					struct epoll_event event = {0};
					event.data.fd = fd;

					if (status.want_read) {
						event.events |= EPOLLIN;
					}
					if (status.want_write) {
						event.events |= EPOLLOUT;
					}
					if (event.events == 0) {
						printf("socket %d closing\n", fd);
						if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL) < 0) {
							perror_die("epoll_ctl EPOLL_CTL_DEL");
						}
						close(fd);
					} else if (epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0) {
						perror_die("epoll_ctl EPOLL_CTL_MOD");
					}
				}
			}
		}
	}

	return 0;
}
