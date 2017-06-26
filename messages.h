#ifndef MESSAGES_H_
#define MESSAGES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UDP_PORT 56700

typedef struct {
	uint16_t size;
	uint16_t protocol : 12;
	bool addressable : 1;
	bool tagged : 1;
	uint8_t origin : 2;
	uint32_t source;
} header_frame_t;

#define MESSAGE_PROTOCOL 1024

typedef struct {
	uint8_t mac[6];
	uint8_t reserved[6];
	bool res_required : 1;
	bool ack_required : 1;
	uint8_t : 6;
	uint8_t sequence;
} header_address_t;

typedef struct {
	uint64_t : 64;
	uint16_t : 16;
	uint16_t type;
} header_protocol_t;

#define MESSAGE_TYPE_GETSERVICE    2
#define MESSAGE_TYPE_STATESERVICE  3
#define MESSAGE_TYPE_GETPOWER     20
#define MESSAGE_TYPE_SETPOWER     21
#define MESSAGE_TYPE_STATEPOWER   22

extern const char *const MESSAGE_TYPES[];
const size_t MESSAGE_TYPES_LEN;

typedef struct {
	header_frame_t frame;
	header_address_t address;
	header_protocol_t protocol;
} message_t;

typedef struct {
	message_t header;
	uint8_t service;
	uint32_t port;
} service_message_t;

#define POWER_LEVEL_STANDBY 0
#define POWER_LEVEL_ENABLED 65535

typedef struct {
	message_t header;
	uint16_t level;
} power_message_t;

void putmsg(message_t *msg);

#endif
