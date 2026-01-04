APP = dpdk-flow-analyzer

DPDK_DIR ?= ../dpdk-stable-24.11.3
CC = gcc

CFLAGS += -O3 -Wall -Wextra
CFLAGS += $(shell pkg-config --cflags libdpdk)

LDFLAGS += $(shell pkg-config --libs libdpdk)

SRCS = src/main.c

all: $(APP)

$(APP): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(LDFLAGS)

clean:
	rm -f $(APP)

