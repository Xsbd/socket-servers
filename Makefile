CC = gcc
CFLAGS = -pthread

EXECUTABLES = \
	      hello-server \
	      hello-client \
	      sequential-server \
	      clients

all: $(EXECUTABLES)

hello-server: sockutils.c hello-server.c
	$(CC) $(CFLAGS) $^ -o $@ 

hello-client: sockutils.c hello-client.c
	$(CC) $(CFLAGS) $^ -o $@

sequential-server: sockutils.c sequential-server.c
	$(CC) $(CFLAGS) $^ -o $@

clients: sockutils.c clients.c
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(EXECUTABLES) *.o
