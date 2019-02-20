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


## clients  (clients.c)
    > Multiple threaded clients doing the same thing.
    > Asume clients are non-malicious, and wait for server ack to initiate communication.
    > Sends random characters
    > Terminates on the reception of the pattern '1111'
###  Performance
    > Evaluates the server response time to complete all tasks of all clients


## servers 

###  0. Protocol
      > Stateful
      > Send '*' to let client know server is ready to receive.
      > Start of receiving data frame identified by '^' delimiter.
      > End of a receiving data frame identified by '$' delimiter.
      > End of stream identified by '0000' pattern; close socket.
      > Increments valid characters by 1 and sends back to client.

      [img] Server State Machine


#### REF
  1. [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/multi/index.html)
	2. [Eli Bendersky's blog](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction/)
