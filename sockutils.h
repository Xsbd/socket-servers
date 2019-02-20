/* header file for INET socket wrapper functions */

#ifndef SOCKUTILS_H
#define SOCKUTILS_H

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Print message to stdout and die (exit with a failure status) */
void die(char* fmt, ...);

/* Wraps malloc with error checking: dies if malloc fails. */
void* xmalloc(size_t size);

/* Dies (exits with a failure status) after printing the current perror status
 * prefixed with msg.
 */
void perror_die(char* msg);

/* Reports a peer connection to stdout. sa is the data populated by a successful
 * accept() call.
 */
void connection_report(const struct sockaddr* sa, socklen_t salen);

/* Creates a bound and listening INET socket on the given port number. Returns
 * the socket fd when successful; dies in case of errors.
 */
int listen_inet(char* portnum);

/* Connect to the INET socket of the given server, port number. Returns
 * the socket fd when successful; dies in case of errors.
 */
int connect_inet(char* server, char* portnum);

#endif /* SOCKUTILS_H */
