#include "devices.h"

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct internal_device {
	char hostname[MAX_HOSTLEN];
	struct sockaddr_in ip;
	uint8_t mac[8];
};

static int devcmp(const struct internal_device *left, const struct internal_device *right) {
	return strcmp(left->hostname, right->hostname);
}

static struct hsearch_data devlookup = {0};
static struct internal_device devs[MAX_DEVICES] = {0};
static size_t numdevs = 0;

bool devdiscover(int socket) {
	static bool idempotency = false;
	if(idempotency)
		return true;
	idempotency = true;

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
	if(sendto(socket, &mess, sizeof mess, 0, (struct sockaddr *) &bcast, sizeof bcast) != sizeof mess) {
		perror("Broadcasting beacon");
		return false;
	}

	if(!hcreate_r(MAX_DEVICES, &devlookup)) {
		perror("Initializing directory");
		return false;
	}

	char (*hostnames)[MAX_HOSTLEN] = malloc(MAX_DEVICES * MAX_HOSTLEN);
	service_message_t resp = {0};
	struct sockaddr_in addr = {0};
	socklen_t addrlen;
	bool found;
	do {
		addrlen = sizeof addr;
		if((found = recvfrom(socket, &resp, sizeof resp, 0, (struct sockaddr *) &addr, &addrlen) != -1)) {
			if(resp.header.protocol.type != MESSAGE_TYPE_STATESERVICE)
				continue;

			putmsg(&resp.header);

			const char *ip = inet_ntoa(addr.sin_addr);
			printf("Network address: %s\n", ip);

			char hostname[128];
			int code;
			if((code = getnameinfo((struct sockaddr *) &addr, addrlen, hostname, sizeof hostname, NULL, 0, 0))) {
				fprintf(stderr, "%s: %s\n", "Querying DNS", gai_strerror(code));
				free(hostnames);
				return false;
			}
			char *dot = strchr(hostname, '.');
			if(dot)
				*dot = '\0';
			printf("Network name: %s\n", hostname);

			ENTRY *ign = NULL;
			if(hsearch_r((ENTRY) {hostname, NULL}, FIND, &ign, &devlookup))
				continue;

			strncpy(devs[numdevs].hostname, hostname, sizeof devs->hostname - 1);
			memcpy(&devs[numdevs].ip, &addr, addrlen);
			memcpy(devs[numdevs].mac, resp.header.address.mac, sizeof devs->mac);
			strncpy((char *) (hostnames + numdevs), hostname, sizeof *hostnames - 1);
			hsearch_r((ENTRY) {hostnames[numdevs], NULL}, ENTER, &ign, &devlookup);
			++numdevs;
		}
	} while(found && numdevs < MAX_DEVICES);

	qsort(devs, numdevs, sizeof *devs, (int (*)(const void *, const void *)) devcmp);
	for(unsigned index = 0; index < numdevs; ++index) {
		ENTRY *crossref = NULL;
		hsearch_r((ENTRY) {devs[index].hostname, NULL}, FIND, &crossref, &devlookup);
		assert(crossref);

		crossref->key = devs[index].hostname;
		crossref->data = devs + index;
	}
	free(hostnames);

	return true;
}

void devcleanup(void) {
	hdestroy_r(&devlookup);
	memset(devs, 0, sizeof devs);
	numdevs = 0;
}

size_t devlist(const device_t **a) {
	*a = (device_t *) devs;
	return numdevs;
}

const device_t *devfind(const char *hostname) {
	ENTRY *res = NULL;
	char key[strlen(hostname) + 1];
	strcpy(key, hostname);

	hsearch_r((ENTRY) {key, NULL}, FIND, &res, &devlookup);
	return res ? res->data : NULL;
}

bool sendpayload(int socket, const device_t *dest, size_t len, message_t *partial) {
	if(!partial->protocol.type)
		return false;

	partial->frame.size = len;
	partial->frame.addressable = true;
	partial->frame.protocol = MESSAGE_PROTOCOL;
	memcpy(partial->address.mac, dest->mac, sizeof partial->address.mac);

	if(sendto(socket, partial, len, 0, (const struct sockaddr *) &dest->ip, sizeof dest->ip)) {
		perror("Sending request");
		return false;
	}
	return true;
}

bool sendall(int socket, size_t numdests, const device_t *const *dests, size_t len, message_t *partial) {
	bool success = true;

	for(unsigned index = 0; index < numdests; ++index)
		success = sendpayload(socket, dests[index], len, partial) && success;
	return success;
}
