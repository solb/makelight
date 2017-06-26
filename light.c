#include "comms.h"
#include "messages.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>

void discover(int socket, bool enable);

int main(int argc, char **argv) {
	if(argc != 2 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
		printf("USAGE: %s <on | off>\n", argv[0]);
		return 1;
	}
	bool enable = !strcmp(argv[1], "on");

	int sock = bind_udp_bcast(UDP_PORT);
	if(sock < 0)
		return 2;

	discover(sock, enable);

	close(sock);
	return 0;
}

void discover(int socket, bool enable) {
	struct sockaddr_in bcast = {
		.sin_family = AF_INET,
		.sin_port = htons(UDP_PORT),
		.sin_addr = htonl(INADDR_BROADCAST),
	};
	message_t mess = {
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

			power_message_t demand = {
				.header = {
					.frame = {
						.size = sizeof demand,
						.addressable = true,
						.protocol = MESSAGE_PROTOCOL,
					},
					.address = {
						.mac = {
							resp.header.address.mac[0],
							resp.header.address.mac[1],
							resp.header.address.mac[2],
							resp.header.address.mac[3],
							resp.header.address.mac[4],
							resp.header.address.mac[5],
						}
					},
					.protocol = {
						.type = MESSAGE_TYPE_SETPOWER,
					}
				},
				.level = enable ? POWER_LEVEL_ENABLED : POWER_LEVEL_STANDBY,
			};
			sendto(socket, &demand, sizeof demand, 0, (struct sockaddr *) &addr, sizeof addr);
		}
	} while(success);
}
