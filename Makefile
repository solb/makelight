CFLAGS   := -std=c99 -g -Og -Werror -Wall -Wextra -Wpedantic -Wno-missing-braces
CPPFLAGS := -DVERBOSE

light: light.o comms.o devices.o messages.o

light.o: CPPFLAGS += -D_POSIX_C_SOURCE=201112L
light.o: comms.h messages.h
devices.o: CPPFLAGS += -D_GNU_SOURCE
devices.o: devices.h messages.h
comms.o: comms.h
messages.o: messages.h
