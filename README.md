# Explore concurrency in servers

## prelim
  1. Vanilla POSIX TCP socket wrapper module to abstract socket creation to socket bind stages.
      > sockutils.h / sockutils.c
  2. 'Hello, servers!' test TCP client-server connection.
      > hello-server.c / hello-client.c
      '''bash
      $ make hello-server
      $ make hello-client
      '''
