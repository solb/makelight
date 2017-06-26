#ifndef COMMS_H_
#define COMMS_H_

#include <netinet/in.h>
#include <unistd.h> // close()

#define RECV_TIMEOUT 1000

int bind_udp_bcast(in_port_t listen);

#endif
