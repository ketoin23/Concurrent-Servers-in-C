#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

#define MAXFDS 1000

typedef enum {INITIAL_ACK, WAIT_FOR_MSG, IN_MSG} ProcessingState;

#define SENDBUF_SIZE 1024

typedef struct {
	ProcessingState state;
	uint8_t sendbuf[SENDBUF_SIZE];
	int sendbuf_end;
	int sendptr;
} peer_state_t;

peer_state_t global_state[MAXFDS];

typedef struct {
	bool want_read;
	bool want_write;
} fd_status_t;

const fd_status_t fd_status_R = {.want_read = true, .want_write = false};
const fd_status_t fd_status_W = {.want_read = false, .want_write = true};
const fd_status_t fd_status_RW = {.want_read = true, .want_write = true};
const fd_status_t fd_status_NORW = {.want_read = false, .want_write = false};

fd_status_t on_peer_connected(int sockfd, const struct sockaddr_in* peer_addr, socklen_t peer_addr_len) {
	assert(sockfd < MAXFDs);
	report_peer_connected(peer_addr, peer_addr_len);

	peer_state_t* peerState = &global_state[sockfd];
	peerState->state = INITIAL_ACK;
	peerState->sendbuf[0] = '*';
	peerState->sendptr = 0;
	peerState->sendbuf_end = 1;

	return fd_status_W;
}

fd_status_t on_peer_ready_recv(int sockfd) {
	assert(sockfd < MAXFDs);
	peer_state_t* peerState = &global_state[sockfd];

	if(peerState->state == INITIAL_ACK || peerState->sendptr < peerState->sendbuf_end) {
		return fd_status_W;
	}

	uint8_t buf[1024];
	int nbytes = recv(sockfd, buf, sizeof buf, 0);
	if(nbytes == 0) {
		return fd_status_NORW;
	} else if(nbytes < 0) {
		if(errno == EAGAIN || errno == EWOULDBLOCK) {
			return fd_status_R;
		} else {
			perror_die("recv");
		}
	}
	bool ready_to_send = false;
	for(int i = 0; i < nbytes; ++i) {
		switch(peerState->state) {
		case INITIAL_ACK:
			assert(0 && "can't reach here");
			break;
		case WAIT_FOR_MSG:
			if(buf[i] == '^') {
				peerState->state = IN_MSG;
			}
			break;
		case IN_MSG:
			if(buf[i] == '$') {
				peerState->state = WAIT_FOR_MSG;
			} else {
				assert(peerState->sendbuf_end < SENDBUF_SIZE);
				peerState->sendbuf[peerState->sendbuf_end++] = buf[i] + 1;
				ready_to_send = true;
			}
			break;
		}
	}

	return (fd_status_t){.want_read = !ready_to_send, .want_write = ready_to_send};
}

fd_status_t on_peer_ready_send(int sockfd) {
	assert(sockfd < MAXFDs);
	peer_state_t* peerState = &global_state[sockfd];

	if(peerState->sendptr >= peerState->sendbuf_end) {
		return fd_status_RW;
	}
	int sendlen = peerState->sendbuf_end - peerState->sendptr;
	int nsent = send(sockfd, &peerState->sendbuf[peerState->sendptr], sendlen, 0);
	if(nsent == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
	      return fd_status_W;
	    } else {
	      perror_die("send");
    	}
	}

	if(nsent < sendlen) {
		peerState->sendptr += nsent;
		return fd_status_W;
	} else {
		peerState->sendptr = 0;
		peerState->sendbuf_end = 0;

		if(peerState->state == INITIAL_ACK) {
			peerState->state = WAIT_FOR_MSG;
		}

		return fd_status_R;
	}
}

int main(int argc, char** argv) {
	setvbuf(stdout, NULL, _IONBF, 0);

	int portnum = 9090;
	if(argc >= 2) {
		portnum = atoi(argv[1]);
	}
	printf("Serving on port %d\n", portnum);
	int listener_sockfd = listen_inet_socket(portnum);

	make_socket_non_blocking(listener_sockfd);
	if(listener_sockfd >= FD_SETSIZE) {
		die("listener socket fd (%d) >= FD_SETSIZE (%d)", listener_sockfd, FD_SETSIZE);
	}	

	fd_set readfds_master;
	FD_ZERO(&readfds_master);
	fd_set writefds_master;
	FD_ZERO(&writefds_master);

	FD_SET(listener_sockfd, &readfds_master);
	int fdset_max = listener_sockfd;

	while(1) {
		fd_set readfds = readfds_master;
		fd_set writefds = writefds_master;

		int nready = select(fdset_max + 1, &readfds, &writefds, NULL, NULL);
		if(nready < 0) {
			perror_die("select");
		}

		for(int fd = 0; fd <= fdset_max && nready > 0; fd++) {
			if(FD_ISSET(fd, &readfds)) {
				nready--;
				if(fd == listener_sockfd) {
					struct sockaddr_in peer_addr;
					socklen_t peer_addr_len = sizeof(peer_addr);
					int newsockfd = accept(listener_sockfd, (struct sockaddr*)&peer_addr, &peer_addr_len);

					if(newsockfd < 0) {
						if(errno == EAGAIN || errno == EWOULDBLOCK) {
							printf("accept returned EAGAIN or EWOULDBLOCK\n");
						} else {
							perror_die("accept");
						}
					} else {
						make_socket_non_blocking(newsockfd);
						if(newsockfd > fdset_max) {
							if(newsockfd >= FD_SETSIZE) {
								die("socket fd (%d) >= FD_SETSIZE (%d)", newsockfd, FD_SETSIZE);
							}
							fdset_max = newsockfd;
						}

						fd_status_t status = on_peer_connected(newsockfd, &peer_addr, peer_addr_len);
						if(status.want_read) {
							FD_SET(newsockfd, &readfds_master);
						} else {
							FD_CLR(newsockfd, &readfds_master);
						}

						if(status.want_write) {
							FD_SET(newsockfd, &writefds_master);
						} else {
							FD_CLR(newsockfd, &writefds_master);
						}
					}
				} else {
					fd_status_t status = on_peer_ready_recv(fd);
					if(status.want_read) {
						FD_SET(fd, &readfds_master);
					} else {
						FD_CLR(fd, &readfds_master);
					}
					if(status.want_write) {
						FD_SET(fd, &writefds_master);
					} else {
						FD_CLR(fd, &writefds_master);
					}

					if(!status.want_write && !status.want_read) {
						printf("socket %d is closing", fd);
						close(fd);
					}
				}
			}
			
			if(FD_ISSET(fd, &writefds)) {
				nready--;
				fd_status_t status = on_peer_ready_send(fd);
				if(status.want_read) {
					FD_SET(fd, &readfds_master);
				} else {
					FD_CLR(fd, &readfds_master);
				}
				if(status.want_write) {
					FD_SET(fd, &writefds_master);
				} else {
					FD_CLR(fd, &writefds_master);
				}
				if(!status.want_write && !status.want_read) {
					printf("socket %d is closing\n", fd);
					close(fd);
				}
			}
		}
	}

	return 0;
}