#ifndef LISOD_H
#define LISOD_H

#include <sys/select.h>
#include <netinet/in.h>
#include "uthash.h"

#define BUF_SIZE  8192
#define LOG_SIZE  1024
#define FREE_SIZE 40

#define NOLIST  1
#define REGF4M  2
#define VIDEO   3

struct serv_rep {
  char response[BUF_SIZE]; // arr of chars containing response from server.

  char* status;
  char* headers;

  char* body;  // alloc memory for body to recv
  ssize_t body_size; // size of body to recv

  int end_idx; // used to mark end of data in response buffer
  int body_idx; // used to mark end of data in body buffer

  int expecting; // What is the server sending me?
};

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
  char  serv_ip[INET_ADDRSTRLEN];   // Store the IP in string form

  int servfd;      // File descriptor of server sock for this client.
  struct serv_rep* servst; // Keep state of the server of this client.

  struct timespec start; // Time of receiving complete chunk request.
  struct timespec end;   // Time of receiving complete chunk data.

  double avg_tput;        // Average tput using EW2MA.
  unsigned long long current_best;
  char lastchunk[300];

  char* freebuf[FREE_SIZE];   // Hold ptrs to any buffer that needs freeing
} fsm;

typedef struct pool {
  int maxfd;         /* Largest descriptor in the master set */

  fd_set masterfds;  /* Set containing all active descriptors */
  fd_set readfds;    /* Subset of descriptors ready for reading */
  fd_set writefds;   /* Subset of descriptors ready for writing */

  int nready;        /* Number of ready descriptors from select */
  int maxi;          /* Max index of clientfd array             */

  int clientfd[FD_SETSIZE];

  char* freebuf[FREE_SIZE];   // Hold ptrs to any buffer that needs freeing */
  fsm* states[FD_SETSIZE];    /* Array of states for each client */

} pool;

struct bitrate {
  int bitrate;
  UT_hash_handle hh;
};

void rm_client(int client_fd, pool* p, int i);
void rm_cgi(int cgi_fd, pool* p, char* logmsg, int i);
void client_error(fsm* state, int error);
void cleanup(int sig);

#endif
