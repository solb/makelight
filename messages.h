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
	uint16_t : 16;
	uint64_t : 48;
	bool res_required : 1;
	bool ack_required : 1;
	uint8_t : 6;
	uint8_t sequence;
} header_address_t;

typedef struct {
	uint64_t : 64;
	uint16_t type;
	uint16_t : 16;
} header_protocol_t;

#define MESSAGE_TYPE_GETSERVICE    2
#define MESSAGE_TYPE_STATESERVICE  3
#define MESSAGE_TYPE_GETPOWER    116
#define MESSAGE_TYPE_SETPOWER    117
#define MESSAGE_TYPE_STATEPOWER  118
#define MESSAGE_TYPE_GET         101
#define MESSAGE_TYPE_SETCOLOR    102
#define MESSAGE_TYPE_STATE       107

extern const char *const MESSAGE_TYPES[];
const size_t MESSAGE_TYPES_LEN;

typedef struct {
	header_frame_t frame;
	header_address_t address;
	header_protocol_t protocol;
} message_t;

typedef struct __attribute__((packed)) {
	message_t header;
	uint8_t service;
	uint32_t port;
} service_message_t;

#define POWER_LEVEL_STANDBY 0
#define POWER_LEVEL_ENABLED 65535

typedef struct __attribute__((packed)) {
	message_t header;
	uint16_t level;
	uint32_t duration;
} power_message_t;

typedef struct {
	message_t header;
	uint16_t level;
} level_message_t;

typedef struct {
	uint16_t hue;
	uint16_t saturation;
	uint16_t brightness;
	uint16_t kelvin;
} hsbk_color_t;

typedef struct __attribute__((packed)) {
	message_t header;
	uint8_t : 8;
	hsbk_color_t color;
	uint32_t duration;
} color_message_t;

typedef struct {
	message_t header;
	hsbk_color_t color;
	uint16_t : 16;
	uint16_t power;
	char label[32];
	uint64_t : 64;
} state_message_t;

void putmsg(message_t *msg);

#endif
