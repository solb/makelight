#include "comms.h"
#include "messages.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

void discover(int socket);

int main(void) {
	int sock = bind_udp_bcast(UDP_PORT);
	if(sock < 0)
		return 2;

	discover(sock);

	close(sock);
	return 0;
}

void discover(int socket) {
	struct sockaddr_in bcast = {
		.sin_family = AF_INET,
		.sin_port = htons(UDP_PORT),
		.sin_addr = htonl(INADDR_BROADCAST),
	};
	header_t mess = {
		.frame = {
			.size = sizeof mess,
			.tagged = true,
			.addressable = true,
			.protocol = MESSAGE_PROTOCOL,
		},
		.protocol = {
			.type = MESSAGE_TYPE_GETSERVICE,
		},
	};
	sendto(socket, &mess, sizeof mess, 0, (struct sockaddr *) &bcast, sizeof bcast);

	service_message_t resp = {0};
	struct sockaddr_in addr = {0};
	socklen_t addrlen;
	bool success;
	do {
		addrlen = sizeof addr;
		success = recvfrom(socket, &resp, sizeof resp, 0, (struct sockaddr *) &addr, &addrlen) != -1;

		if(success && resp.header.protocol.type == MESSAGE_TYPE_STATESERVICE) {
			putmsg(&resp.header);

			const char *ip = inet_ntoa(addr.sin_addr);
			printf("Network address: %s\n", ip);

			char hostname[128];
			getnameinfo((struct sockaddr *) &addr, addrlen, hostname, sizeof hostname, NULL, 0, 0);
			char *dot = strchr(hostname, '.');
			if(dot)
				*dot = '\0';
			printf("Network name: %s\n", hostname);
		}
	} while(success);
}
