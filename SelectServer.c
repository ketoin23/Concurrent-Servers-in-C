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

fd_status_peer_recv(int sockfd) {
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
}