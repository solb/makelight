#include "comms.h"
#include "devices.h"

#include <readline/readline.h>
#include <assert.h>
#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LONGEST_CMD 5

static const char COMMAND_PROMPT[] = "@ ";
static const char COMMAND_PROMPT_FAILED[] = "! ";

static bool list(void);
static bool on(void);
static bool off(void);

static const char COMMAND_QUIT[] = "quit";
static struct {
	char keyword[LONGEST_CMD];
	bool (*const fun)(void);
} COMMANDS[] = {
	{"list", list},
	{"on", on},
	{"off", off},
};
static size_t NCOMMANDS = sizeof COMMANDS / sizeof *COMMANDS;

static int sock;

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

	if(!devlist(NULL)) {
		fputs("FAILURE to find any devices!\n", stderr);
		devcleanup();
		close(sock);
		return 4;
	}

	shell();

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

		ENTRY *rule = hsearch((ENTRY) {line, NULL}, FIND);
		if(rule) {
			bool (*fun)(void) = (bool (*)(void)) (uintptr_t) rule->data;
			assert(fun);

			lastline = line;
			success = fun();
		}
	} while(strcmp(line, quit));

	free(quit);
	hdestroy();
}

static bool power(size_t count, const device_t *dests, bool on);

static bool on(void) {
	return power(0, NULL, true);
}

static bool off(void) {
	return power(0, NULL, false);
}

static bool list(void) {
	const device_t *lista = NULL;
	size_t listlen = devlist(&lista);

	puts("Found the following lights:");
	for(unsigned index = 0; index < listlen; ++index)
		printf("\t%s\n", lista[index].hostname);

	return true;
}

static bool power(size_t count, const device_t *dests, bool on) {
	power_message_t request = {
		.header.protocol.type = MESSAGE_TYPE_SETPOWER,
		.level = on ? POWER_LEVEL_ENABLED : POWER_LEVEL_STANDBY,
	};

	return sendall(sock, count, dests, sizeof request, &request.header, NULL);
}
