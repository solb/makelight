#include "messages.h"

#include <assert.h>
#include <stdio.h>

const char *const MESSAGE_TYPES[] = {
	[MESSAGE_TYPE_GETSERVICE] = "GetService",
	[MESSAGE_TYPE_STATESERVICE] = "StateService",
	[MESSAGE_TYPE_GETPOWER] = "GetPower",
	[MESSAGE_TYPE_SETPOWER] = "SetPower",
	[MESSAGE_TYPE_STATEPOWER] = "StatePower",
};
const size_t MESSAGE_TYPES_LEN = sizeof MESSAGE_TYPES / sizeof *MESSAGE_TYPES;

static void decode_service(const header_t *header) {
	assert(header->protocol.type == MESSAGE_TYPE_STATESERVICE);

	const service_message_t *message = (const service_message_t *) header;
	printf("\tService: %u\n\tPort: %u\n", message->service, message->port);
}

static void decode_power(const header_t *header) {
	assert(header->protocol.type == MESSAGE_TYPE_SETPOWER ||
		header->protocol.type == MESSAGE_TYPE_STATEPOWER);

	const power_message_t *message = (const power_message_t *) header;
	printf("\tLevel: %u\n", message->level);
}

static void (*const DECODER_FUNS[])(const header_t *) = {
	[MESSAGE_TYPE_STATESERVICE] = decode_service,
	[MESSAGE_TYPE_SETPOWER] = decode_power,
	[MESSAGE_TYPE_STATEPOWER] = decode_power,
};
static const size_t DECODER_FUNS_LEN = sizeof DECODER_FUNS / sizeof *DECODER_FUNS;

void putmsg(header_t *header) {
	uint16_t type = header->protocol.type;
	const char *typename = type < MESSAGE_TYPES_LEN ? MESSAGE_TYPES[type] : NULL;
	printf("LIFX %s message\n", typename);

	puts("\tFrame");
	printf("\t\tSize: %u\n\t\tOrigin: %u\n\t\tTagged: %d\n\t\tAddressable: %d\n\t\tProtocol: %u\n\t\tSource: %u\n",
		header->frame.size,
		header->frame.origin,
		header->frame.tagged,
		header->frame.addressable,
		header->frame.protocol,
		header->frame.source);

	puts("\tAddress");
	printf("\t\tTarget: %02x:%02x:%02x:%02x:%02x:%2x\n\t\tAcknowledgement required: %d\n\t\tResponse required: %d\n\t\tSequence: %u\n",
		header->address.mac[0],
		header->address.mac[1],
		header->address.mac[2],
		header->address.mac[3],
		header->address.mac[4],
		header->address.mac[5],
		header->address.ack_required,
		header->address.res_required,
		header->address.sequence);

	puts("\tProtocol");
	printf("\t\tType: %u (%s)\n", type, typename);

	void (*decode_fun)(const header_t *) = type < DECODER_FUNS_LEN ? DECODER_FUNS[type] : NULL;
	if(decode_fun)
		decode_fun(header);
}
