#ifndef DEVICES_H_
#define DEVICES_H_

#include "messages.h"

#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_DEVICES  32
#define MAX_HOSTLEN 128

typedef struct {
	const char hostname[MAX_HOSTLEN];
	const struct sockaddr_in ip;
	const uint8_t mac[6];
	const hsbk_color_t color;
	uint16_t power;
} device_t;

bool devdiscover(int socket);
void devcleanup(void);

/* Client code may choose to make a single copy of each of the device_t instances returned by these
 * functions, and may even pass the resulting copies into this module's other functions, provided
 * it makes no attempt to change them.  This module will keep its internal copies and any other copy
 * that is provided to one of its mutating copies up to date with current state information.
 */

/* If a is NULL, only the cardinality will be returned. */
size_t devlist(const device_t **a);
const device_t *devfind(const char *hostname);

#define DEVICE_CMASK_HUE 0x1
#define DEVICE_CMASK_SAT 0x2
#define DEVICE_CMASK_VAL 0x4
#define DEVICE_CMASK_KEL 0x8

typedef bool (*send_callback_t)(unsigned index, const device_t *dest);

/* The partial message will first be modified in-place to add all required header fields, leaving
 * all other fields intact.  The caller need only zero-initialize the message header, initialize its
 * protocol.type field, and provide any applicable payload fields; however, it may also choose to
 * provide values for one or more of the optional header fields.
 */
bool sendpayload(int socket, const device_t *dest, ssize_t len, message_t *partial, uint8_t cmask);
/* A dests of NULL means all devices. */
bool sendall(int socket, size_t numdests, const device_t *dests, ssize_t len, message_t *partial, uint8_t cmask, send_callback_t cb);

/* Discards pending messages until it either receives one of the expected type or times out in the
 * attempt.  Note that in the latter case, buf will be populated with (possibly only the beginning
 * of) the last message so discarded, if any.
 */
bool expect(int socket, ssize_t len, message_t *buf, uint16_t type);
bool expectwhence(int socket, ssize_t len, message_t *buf, uint16_t type, struct sockaddr_in *src);

#endif
