CC = gcc
CCFLAGS = -std=gnu99 -Wall -O3 -g -DNDEBUG -pthread
LDFLAGS = -lpthread -pthread

EpollServer: utils.c EpollServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

SelectServer: utils.c SelectServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

ThreadedServer: utils.c ThreadedServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)

SequentialServer: utils.c SequentialServer.c
	$(CC) $(CCFLAGS) $^ -o $@ $(LDFLAGS)