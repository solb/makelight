#include "comms.h"
#include "devices.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <assert.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LONGEST_CMD 10
#define COMMAND_DELIM ':'

static const char COMMAND_PROMPT[] = "@ ";
static const char COMMAND_PROMPT_FAILED[] = "! ";

static bool list(const char *arg);
static bool host(const char *arg);
static bool all(const char *arg);
static bool on(const char *arg);
static bool off(const char *arg);
static bool toggle(const char *arg);
static bool hue(const char *arg);
static bool saturation(const char *arg);
static bool brightness(const char *arg);
static bool kelvin(const char *arg);

static const char COMMAND_QUIT[] = "quit";
static struct {
	char keyword[LONGEST_CMD];
	bool (*const fun)(const char *arg);
} COMMANDS[] = {
	{"list", list},
	{"host", host},
	{"all", all},
	{"on", on},
	{"off", off},
	{"toggle", toggle},
	{"hue", hue},
	{"saturation", saturation},
	{"brightness", brightness},
	{"kelvin", kelvin},
};
static size_t NCOMMANDS = sizeof COMMANDS / sizeof *COMMANDS;

static int sock;
static size_t ndevices;
static const device_t *devices;

static void shell(void);

int main(int argc, char **argv) {
	if(argc >= 2 && !strcmp(argv[1], "--help")) {
		printf("USAGE: %s [command line]...\n", argv[0]);
		return 1;
	}

	sock = bind_udp_bcast(UDP_PORT);
	if(sock < 0) {
		fputs("FAILURE during network initialization!\n", stderr);
		return 2;
	}

	if(!devdiscover(sock)) {
		fputs("FAILURE during device discovery!\n", stderr);
		close(sock);
		return 3;
	}

	const device_t *list = NULL;
	if(!(ndevices = devlist(&list))) {
		fputs("FAILURE to find any devices!\n", stderr);
		devcleanup();
		close(sock);
		return 4;
	}
	devices = list;

	shell();

	if(devices != list)
		free((device_t *) devices);
	devcleanup();
	close(sock);
	return 0;
}

static void shell(void) {
	hcreate(NCOMMANDS);
	for(unsigned index = 0; index < NCOMMANDS; ++index)
		hsearch((ENTRY) {COMMANDS[index].keyword, (void *) (uintptr_t) COMMANDS[index].fun}, ENTER);

	char *quit = strdup(COMMAND_QUIT);

	char *line = NULL;
	char *lastline = "";
	bool success = true;
	do {
		line = readline(success ? COMMAND_PROMPT : COMMAND_PROMPT_FAILED);

		if(!line)
			line = quit;
		else if(!strlen(line))
			line = lastline;
		else
			add_history(line);

		char *delim = strchr(line, COMMAND_DELIM);
		if(delim)
			*delim = '\0';

		success = false;
		ENTRY *rule = hsearch((ENTRY) {line, NULL}, FIND);
		if(rule) {
			bool (*fun)(const char *) = (bool (*)(const char *)) (uintptr_t) rule->data;
			assert(fun);

			lastline = line;
			success = fun(delim ? (delim + 1) : NULL);

			if(delim)
				*delim = COMMAND_DELIM;
		}
	} while(strcmp(line, quit));

	free(quit);
	hdestroy();
}

static bool list(const char *arg) {
	(void) arg;

	const device_t *lista = NULL;
	size_t listlen = devlist(&lista);

	puts("Found the following lights:");
	for(unsigned index = 0; index < listlen; ++index)
		printf("\t%s\n", lista[index].hostname);

	return true;
}

static bool host(const char *arg) {
	const device_t *dev = devfind(arg);
	if(!dev) {
		fputs("Couldn't find a device by that name!", stderr);
		return false;
	}

	const device_t *list = NULL;
	devlist(&list);
	if(devices == list) {
		devices = NULL;
		ndevices = 0;
	}

	device_t *extended = realloc((device_t *) devices, (ndevices + 1) * sizeof *devices);
	memcpy(extended + ndevices, dev, sizeof *devices);
	devices = extended;
	++ndevices;

	return true;
}

static bool all(const char *arg) {
	(void) arg;

	const device_t *list = NULL;
	ndevices = devlist(&list);

	if(devices != list)
		free((device_t *) devices);
	devices = list;

	return true;
}

static bool power(size_t count, const device_t *dests, bool on);

static bool on(const char *arg) {
	(void) arg;
	return power(ndevices, devices, true);
}

static bool off(const char *arg) {
	(void) arg;
	return power(ndevices, devices, false);
}

static bool toggle(const char *arg) {
	(void) arg;

	bool anyon = false;
	for(unsigned index = 0; index < ndevices; ++index)
		if(devices[index].power) {
			anyon = true;
			break;
		}

	return power(ndevices, devices, !anyon);
}

static bool hue(const char *arg) {
	color_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETCOLOR,
		.color.hue = atoi(arg),
	};
	return sendall(sock, ndevices, devices, sizeof request, &request.header, DEVICE_CMASK_HUE, NULL);
}

static bool saturation(const char *arg) {
	color_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETCOLOR,
		.color.saturation = atoi(arg),
	};
	return sendall(sock, ndevices, devices, sizeof request, &request.header, DEVICE_CMASK_SAT, NULL);
}

static bool brightness(const char *arg) {
	color_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETCOLOR,
		.color.brightness = atoi(arg),
	};
	return sendall(sock, ndevices, devices, sizeof request, &request.header, DEVICE_CMASK_VAL, NULL);
}

static bool kelvin(const char *arg) {
	color_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETCOLOR,
		.color.kelvin = atoi(arg),
	};
	return sendall(sock, ndevices, devices, sizeof request, &request.header, DEVICE_CMASK_KEL | DEVICE_CMASK_SAT, NULL);
}

static bool power(size_t count, const device_t *dests, bool on) {
	power_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETPOWER,
		.level = on ? POWER_LEVEL_ENABLED : POWER_LEVEL_STANDBY,
	};

	return sendall(sock, count, dests, sizeof request, &request.header, 0, NULL);
}
