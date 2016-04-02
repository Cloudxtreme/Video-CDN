#ifndef LISOD_H
#define LISOD_H

#include <sys/select.h>
#include <openssl/ssl.h>
#include <netinet/in.h>

#define BUF_SIZE  8192
#define LOG_SIZE  1024
#define FREE_SIZE 40

typedef struct state {
  char request[BUF_SIZE]; // arr of chars containing the text of the request.
  char response[BUF_SIZE]; // arr of chars containing response to client.

  char* method; // index into method
  char* uri;    // index into uri
  char* version; // you get the idea
  char* header;  // index into the headers

  char* body;  // alloc memory for body to send
  ssize_t body_size; // size of body to send

  int end_idx; // used to mark end of data in buffer
  int resp_idx; // used to mark end of response buffer

  int   conn;      // 1 = keep-alive; 0 = close
  char  cli_ip[INET_ADDRSTRLEN];   // Store the IP in string form

  int   pipefds;           //  file descriptor of script  to be added to select
  char* freebuf[FREE_SIZE];   // Hold ptrs to any buffer that needs freeing
} fsm;

typedef struct pool {
  int maxfd;         /* Largest descriptor in the master set */

  fd_set masterfds;  /* Set containing all active descriptors */
  fd_set readfds;    /* Subset of descriptors ready for reading */
  fd_set writefds;   /* Subset of descriptors ready for writing */

  int nready;        /* Number of ready descriptors from select */
  int maxi;          /* Max index of clientfd array             */

  int clientfd[FD_SETSIZE];   /* Array of active client descriptors */
  fsm* states[FD_SETSIZE]; /* Array of states for each client */
  // char data[FD_SETSIZE][BUF_SIZE];   /* Array that contains data from client */

} pool;

void rm_client(int client_fd, pool* p, char* logmsg, int i);
void rm_cgi(int cgi_fd, pool* p, char* logmsg, int i);
void client_error(fsm* state, int error);
void cleanup(int sig);

#endif
