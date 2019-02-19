CC = gcc

EXECUTABLES = \
	      hello-server \
	      hello-client

all: $(EXECUTABLES)

hello-server: sockutils.c hello-server.c
	$(CC) $^ -o $@

hello-client: sockutils.c hello-client.c
	$(CC) $^ -o $@

.PHONY: clean

clean:
	rm -f $(EXECUTABLES) *.o
