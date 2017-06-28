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
	hsbk_color_t color;
	uint16_t power;
};

static int devcmp(const struct internal_device *left, const struct internal_device *right) {
	return strcmp(left->hostname, right->hostname);
}

static struct hsearch_data devlookup = {0};
static struct internal_device devs[MAX_DEVICES] = {0};
static size_t numdevs = 0;

bool devdiscover(int socket) {
	if(numdevs)
		return true;

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
	bool found;
	do {
		service_message_t resp = {0};
		struct sockaddr_in addr = {0};
		if((found = expectwhence(socket, sizeof resp, &resp.header, MESSAGE_TYPE_STATESERVICE, &addr))) {
			char hostname[MAX_HOSTLEN];
			int code;
			if((code = getnameinfo((struct sockaddr *) &addr, sizeof addr, hostname, sizeof hostname, NULL, 0, 0))) {
				fprintf(stderr, "%s: %s\n", "Querying DNS", gai_strerror(code));
				devcleanup();
				free(hostnames);
				return false;
			}
			char *dot = strchr(hostname, '.');
			if(dot)
				*dot = '\0';

			ENTRY *ign = NULL;
			if(hsearch_r((ENTRY) {hostname, NULL}, FIND, &ign, &devlookup))
				continue;

#ifdef VERBOSE
			const char *ip = inet_ntoa(addr.sin_addr);
			printf("%s (%s): ", hostname, ip);
			putmsg(&resp.header);
#endif

			strncpy(devs[numdevs].hostname, hostname, sizeof devs->hostname - 1);
			memcpy(&devs[numdevs].ip, &addr, sizeof devs[numdevs].ip);
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

		message_t query = {.protocol.type = MESSAGE_TYPE_GET};
		if(!sendpayload(socket, (const device_t *) (devs + index), sizeof query, &query, 0)) {
			perror("Querying state");
			devcleanup();
			free(hostnames);
			return false;
		}

		state_message_t status = {0};
		if(!expect(socket, sizeof status, &status.header, MESSAGE_TYPE_STATE)) {
			perror("Receiving status");
			devcleanup();
			free(hostnames);
			return false;
		}

#ifdef VERBOSE
		printf("%s: ", devs[index].hostname);
		putmsg(&status.header);
#endif

		memcpy(&devs[index].color, &status.color, sizeof devs[index].color);
		devs[index].power = status.power;
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
	if(a)
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

bool sendpayload(int socket, const device_t *dest, ssize_t len, message_t *partial, uint8_t cmask) {
	if(!partial->protocol.type)
		return false;

	partial->frame.size = len;
	partial->frame.addressable = true;
	partial->frame.protocol = MESSAGE_PROTOCOL;
	memcpy(partial->address.mac, dest->mac, sizeof partial->address.mac);
	if(partial->protocol.type == MESSAGE_TYPE_SETCOLOR) {
		color_message_t *message = (color_message_t *) partial;
		if(!(cmask & DEVICE_CMASK_HUE))
			message->color.hue = dest->color.hue;
		if(!(cmask & DEVICE_CMASK_SAT))
			message->color.saturation = dest->color.saturation;
		if(!(cmask & DEVICE_CMASK_VAL))
			message->color.brightness = dest->color.brightness;
		if(!(cmask & DEVICE_CMASK_KEL))
			message->color.kelvin = dest->color.kelvin;
	}

	if(sendto(socket, partial, len, 0, (const struct sockaddr *) &dest->ip, sizeof dest->ip) < len) {
		perror("Sending request");
		return false;
	}

	if(!(partial->protocol.type == MESSAGE_TYPE_STATE ||
		partial->protocol.type == MESSAGE_TYPE_STATEPOWER))
		return true;

	struct internal_device *external = (struct internal_device *) dest;
	ENTRY *handle = NULL;
	hsearch_r((ENTRY) {external->hostname, NULL}, FIND, &handle, &devlookup);
	assert(handle);
	struct internal_device *internal = (struct internal_device *) handle->data;

	if(partial->protocol.type == MESSAGE_TYPE_STATE) {
		state_message_t *message = (state_message_t *) partial;
		memcpy(&internal->color, &message->color, sizeof internal->color);
		internal->power = message->power;
	} else if(partial->protocol.type == MESSAGE_TYPE_STATEPOWER) {
		level_message_t *message = (level_message_t *) partial;
		internal->power = message->level;
	}

	if(external != internal)
		memcpy(external, internal, sizeof *external);

	return true;
}

bool sendall(int socket, size_t numdests, const device_t *dests, ssize_t len, message_t *partial, uint8_t cmask, send_callback_t cb) {
	bool success = true;
	if(!dests)
		numdests = devlist(&dests);

	for(unsigned index = 0; index < numdests; ++index) {
		success = sendpayload(socket, dests + index, len, partial, cmask) && success;
		if(cb)
			success = cb(index, dests) && success;
	}
	return success;
}

bool expect(int socket, ssize_t len, message_t *buf, uint16_t type) {
	struct sockaddr_in ign = {0};
	return expectwhence(socket, len, buf, type, &ign);
}

bool expectwhence(int socket, ssize_t len, message_t *buf, uint16_t type, struct sockaddr_in *src) {
	ssize_t got;
	do {
		socklen_t srclen = sizeof *src;
		got = recvfrom(socket, buf, len, 0, (struct sockaddr *) src, &srclen);
	} while(got >= (intptr_t) &buf->protocol.type - (intptr_t) buf + (intptr_t) sizeof buf->protocol.type &&
		buf->protocol.type != type);

	return buf->protocol.type == type && got == len;
}
