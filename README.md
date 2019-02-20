## Explore concurrency in servers


##### Install Instructions
   run the make file    
    $ make [program]    


### prelim
  1. Vanilla POSIX TCP socket wrapper module to abstract socket creation to socket bind stages.   
      sockutils.c / sockutils.h (api)   
  2. 'Hello, servers!' test TCP client-server connection.   
      hello-server.c / hello-client.c   
  3. Simple threadpool implementation in threadpool.c / threadpool.h (api)


### clients  (clients.c)
    > Multiple threaded clients doing the same thing.   
    > Asume clients are non-malicious, and wait for server ack to initiate communication.   
    > Sends random characters   
    > Terminates on the reception of the pattern '1111'   
####  Performance
    > Evaluates the server response time to complete all tasks of all clients   


### servers 

####  Protocol
    > Stateful    
    > Send '*' to let client know server is ready to receive.   
    > Start of receiving data frame identified by '^' delimiter.    
    > End of a receiving data frame identified by '$' delimiter.    
    > End of stream identified by '0000' pattern; close socket.   
    > Increments valid characters by 1 and sends back to client.    

####  Server properties and issues:
  1. sequential-server.c    
   --> handle one client at a time    
   --> impractical, long wait times for clients   
   Usage:   
          $ ./sequential-server   

  2. threaded-server.c    
   --> handle multiple clients, with one thread per client    
   --> too many threads, lame duck to DoS attack    
   Usage:   
          $ ./threaded-server [port_num]    

  3. threadpool-server.c    
   --> use a fixed number of threads (thread-pool) to process client requests   
   --> better resource management as compared to 1 thread per client    
   Usage:   
          $ ./threadpool-server [port_num] [num_of_threads]   



##### REF
  [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/html/multi/index.html)    
	[Eli Bendersky's blog](https://eli.thegreenplace.net/2017/concurrent-servers-part-1-introduction)     
  [Washington U: cse451](https://courses.cs.washington.edu/courses/cse451/01wi/projects/proj2/proj2.html)    
