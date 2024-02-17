CC = gcc
CCFLAGS = -std=gnu99 -Wall -O3 -g -DNDEBUG -pthread
LDFLAGS = -lpthread -pthread

LDLIBUV = -luv -Wl,-rpath=/usr/local/lib

libuvServer: utils.c libuvServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBUV)

EpollServer: utils.c EpollServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

SelectServer: utils.c SelectServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

ThreadedServer: utils.c ThreadedServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

SequentialServer: utils.c SequentialServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)