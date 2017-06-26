#include "comms.h"

#include <sys/socket.h>
#include <sys/time.h>
#include <stdbool.h>
#include <stdio.h>

int bind_udp_bcast(in_port_t listen) {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		perror("Opening socket");
		return -1;
	}

	int bcast = true;
	if(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof bcast)) {
		perror("Enabling broadcast");
		close(sock);
		return -1;
	}

	struct timeval tout = {
		.tv_sec = RECV_TIMEOUT / 1000,
		.tv_usec = RECV_TIMEOUT % 1000 * 1000,
	};
	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tout, sizeof tout)) {
		perror("Setting timeout");
		close(sock);
		return -1;
	}

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(listen),
		.sin_addr = htonl(INADDR_ANY),
	};
	if(bind(sock, (struct sockaddr *) &addr, sizeof addr)) {
		perror("Binding port");
		close(sock);
		return -1;
	}

	return sock;
}
