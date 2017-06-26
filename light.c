#include "comms.h"
#include "devices.h"

#include <stdio.h>
#include <string.h>

static int sock;

bool power(size_t count, const device_t *const *dest, bool on);

int main(int argc, char **argv) {
	if(argc != 2 || (strcmp(argv[1], "on") && strcmp(argv[1], "off"))) {
		printf("USAGE: %s <on | off>\n", argv[0]);
		return 1;
	}
	const char *verb = argv[1];
	bool enable = !strcmp(verb, "on");

	sock = bind_udp_bcast(UDP_PORT);
	if(sock < 0)
		return 2;

	if(!devdiscover(sock)) {
		fputs("FAILURE during device discovery!\n", stderr);
		close(sock);
		return 3;
	}

	const device_t *list = NULL;
	size_t listlen = devlist(&list);
	if(!listlen) {
		fputs("FAILURE to find any devices!\n", stderr);
		devcleanup();
		close(sock);
		return 4;
	}

	puts("Found the following lights:");
	for(unsigned index = 0; index < listlen; ++index)
		printf("\t%s\n", list[index].hostname);

	printf("Turning all lights %s...\n", verb);
	if(!power(listlen, &list, enable)) {
		fputs("FAILURE to perform the requested operation!\n", stderr);
		devcleanup();
		close(sock);
		return 5;
	}

	devcleanup();
	close(sock);
	return 0;
}

bool power(size_t count, const device_t *const *dest, bool on) {
	power_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETPOWER,
		.level = on ? POWER_LEVEL_ENABLED : POWER_LEVEL_STANDBY,
	};

	return sendall(sock, count, dest, sizeof request, &request.header, NULL);
}
