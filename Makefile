CC = gcc
CFLAGS = -pthread

EXECUTABLES = \
	      hello-server \
	      hello-client \
	      sequential-server \
	      clients \
	      threaded-server \
	      threadpool-server \
	      select-server

all: $(EXECUTABLES)

hello-server: sockutils.c hello-server.c
	$(CC) $(CFLAGS) $^ -o $@ 

hello-client: sockutils.c hello-client.c
	$(CC) $(CFLAGS) $^ -o $@

sequential-server: sockutils.c sequential-server.c
	$(CC) $(CFLAGS) $^ -o $@

clients: sockutils.c clients.c
	$(CC) $(CFLAGS) $^ -o $@

threaded-server: sockutils.c threaded-server.c
	$(CC) $(CFLAGS) $^ -o $@

threadpool-server: sockutils.c threadpool.c threadpool-server.c
	$(CC) $(CFLAGS) $^ -o $@

select-server: sockutils.c select-server.c
	$(CC) $(CFLAGS) $^ -o $@

epoll-server: sockutils.c epoll-server.c
	$(CC) $(CFLAGS) $^ -o $@

.PHONY: clean

clean:
	rm -f $(EXECUTABLES) *.o
