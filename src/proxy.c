/********************************************************************************/
/* @file lisod.c                                                                */
/*                                                                              */
/* @brief A simple web server that uses select() to handle                      */
/* multiple concurrent clients.                                                 */
/*                                                                              */
/* Supports SSL/TLS, CGI, GET, HEAD and POST requests.                          */
/*                                                                              */
/* @author Fadhil Abubaker                                                      */
/*                                                                              */
/* @usage: ./lisod <HTTP port> <HTTPS port> <log file> <lock file> <www folder> */
/* <CGI script path> <privatekey file> <certificate file>                       */
/********************************************************************************/


/* Part of the code is based on the select-based echo server found in
   CSAPP */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "lisod.h"
#include "logger.h"
#include "engine.h"
#include "parse.h"
#include "uthash.h"

/** Global vars **/
FILE* logfile;
float alpha;

short listen_port;
char* fake_ip;
char* dns_ip;
short dns_port;
char* www_ip;

/** Prototypes **/

int  close_socket(int sock);
void init_pool(int listenfd, int https_fd, pool *p);
void add_client(int client_fd, char* wwwfolder, SSL* client_context, pool *p);
void check_clients(pool *p);
void cleanup(int sig);
void sigchld_handler(int sig);
void connect_server(fsm* state);

/** Definitions **/

int main(int argc, char* argv[])
{
  if (argc != 7 && argc != 8) // argc = 9
  {
    fprintf(stderr, "%d \n", argc);
    fprintf(stderr, "usage: %s <log> <alpha> <listen-port> ", argv[0]);
    fprintf(stderr, "<fake-ip> <dns-ip> <dns-port> ");
    fprintf(stderr, "<www-ip> \n");
    return EXIT_FAILURE;
  }

  /* Ignore SIGPIPE */
  signal(SIGPIPE, SIG_IGN);

  /* Handle SIGINT to cleanup after liso */
  signal(SIGINT,  cleanup);

  /* Install SIGCHLD handler to reap children */
  signal(SIGCHLD, sigchld_handler);

  /* Parse cmdline args */
  logfile           = log_open(argv[1]);
  alpha             = atof(argv[2]);
  listen_port       = atoi(argv[3]);
  fake_ip           = argv[4];
  dns_ip            = argv[5];
  dns_port          = atoi(argv[6]);
  if(argc == 8)     www_ip = argv[7];

  /* Various buffers for read/write */
  char log_buf[LOG_SIZE]            = {0};
  char hostname[LOG_SIZE]           = {0};
  char serv_ip[INET_ADDRSTRLEN]     = {0};
  char port[10]                     = {0};

  int                 listen_fd, client_fd;
  socklen_t           cli_size;
  struct sockaddr_in  serv_addr, cli_addr;
  pool *pool =        malloc(sizeof(struct pool));
  struct timeval      tv;
  tv.tv_sec = 5;

  /********* BEGIN INIT *******/

  if(pool == NULL)
  {
    return EXIT_FAILURE;
  }

  fprintf(stdout, "-----Welcome to Proxy!-----\n");

  /* all networked programs must create a socket */
  if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
  {
    return EXIT_FAILURE;
  }

  /* These will help a client connect to the proxy */
  serv_addr.sin_family        = AF_INET;
  serv_addr.sin_port          = htons(listen_port);
  serv_addr.sin_addr.s_addr   = INADDR_ANY;

  /* Set sockopt so that ports can be resued */
  int enable = -1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                 sizeof(int)) == -1)
  {
    close_socket(listen_fd);
    return EXIT_FAILURE;
  }

  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(listen_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)))
  {
    close_socket(listen_fd);
    return EXIT_FAILURE;
  }

  if (listen(listen_fd, 5))
  {
    close_socket(listen_fd);
    return EXIT_FAILURE;
  }

  /* Initialize our pool of fds */
  init_pool(listen_fd, pool);

  /******** END INIT *********/

  /******* BEGIN SERVER CODE ******/

  /* finally, loop waiting for input and then write it back */
  while (1)
  {
    /* Block until there are file descriptors ready */
    pool->readfds = pool->masterfds;
    pool->writefds = pool->masterfds;

    if((pool->nready = select(pool->maxfd+1, &pool->readfds, &pool->writefds,
                              NULL, &tv)) == -1)
    {
      close_socket(listen_fd);
      return EXIT_FAILURE;
    }

    /* Is the http port having clients ? */
    if (FD_ISSET(listen_fd, &pool->readfds))
    {
      cli_size = sizeof(cli_addr);
      if ((client_fd = accept(listen_fd, (struct sockaddr *) &cli_addr,
                              &cli_size)) == -1)
      {
        close(listen_fd);
        return EXIT_FAILURE;
      }

      add_client(client_fd, pool);
    }

    /* Read and respond to each client requests */
    check_clients(pool);
  }
}

int close_socket(int sock)
{
  if (close(sock))
  {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/*
 * @brief Initializes the pool struct so that it contains only the listenfd
 *
 * @param listenfd The socket for listening for new connections.
 * @paran p        The pool struct to initialize.
 */
void init_pool(int listenfd, int https_fd, pool *p)
{
  p->maxi = -1;

  memset(p->clientfd, -1, FD_SETSIZE*sizeof(int)); // No clients at the moment.
  //memset(p->data,      0, FD_SETSIZE*BUF_SIZE*sizeof(char)); // No data yet.
  memset(p->states,    0, FD_SETSIZE*sizeof(fsm*)); // NULL out the fsms.

  /* Initailly, listenfd and https_fd are the only members of the read set */
  p->maxfd = listenfd;
  FD_ZERO(&p->masterfds);
  FD_SET(listenfd, &p->masterfds);
  FD_SET(https_fd, &p->masterfds);
}

/*
 * @brief Adds a client file descriptor to the pool and updates it.
 *
 * @param client_fd The client file descriptor.
 * @param p         The pool struct to update.
 */
void add_client(int client_fd, pool *p)
{
  int i; fsm* state;

  p->nready--;

  /* Create a fsm for this client */
  state = malloc(sizeof(struct state));

  /* Create initial values for fsm */
  memset(state->request,  0, BUF_SIZE);
  memset(state->response, 0, BUF_SIZE);
  state->method     = NULL;
  state->uri        = NULL;
  state->version    = NULL;
  state->header     = NULL;
  state->body       = NULL;
  state->body_size  = -1; // No body as of yet

  state->end_idx    = 0;
  state->resp_idx   = 0;

  state->conn       = 1;
  bzero(state->cli_ip, INET_ADDRSTRLEN);

  connect_server(state);

  state->all_bitrates = NULL;

  memset(state->freebuf, 0, FREE_SIZE*sizeof(char*));

  for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
  {
    if(p->clientfd[i] < 0) {   /* Found one free slot */
      p->clientfd[i] = client_fd;

      /* Add the descriptor to the master set */
      FD_SET(client_fd, &p->masterfds);

      /* Add fsm to pool */
      p->states[i] = state;

      /* Update max descriptor and max index */
      if (client_fd > p->maxfd)
        p->maxfd = client_fd;
      if (i > p->maxi)
        p->maxi = i;
      break;
    }
  }

  if (i == FD_SETSIZE)   /* There are no empty slots */
  {
    client_error(state, 503);
    send(client_fd, state->response, state->resp_idx, 0);
    close_socket(client_fd);
    free(state);
  }
}

/*********************************************************************/
/* @brief Iterates through active clients and reads requests.        */
/*                                                                   */
/* Uses select to determine whether clients are ready for reading or */
/* writing, reads a request. Never blocks for a                      */
/* single user.                                                      */
/*                                                                   */
/* @param p The pool of clients to iterate through.                  */
/*********************************************************************/
void check_clients(pool *p)
{
  int i, client_fd, cgi_fd, n, error;
  fsm* state;
  char buf[BUF_SIZE] = {0}; char log_buf[LOG_SIZE] = {0};
  struct serv_rep* servst;

  memset(buf,0,BUF_SIZE);

  /* Iterate through all clients, and read their data */
  for(i = 0; (i <= p->maxi) && (p->nready > 0); i++)
  {
    if((client_fd = p->clientfd[i]) <= 0)
      continue;

    state     = p->states[i];

    /* If a descriptor is ready to be read, read a line from it */
    if (client_fd > 0 && FD_ISSET(client_fd, &p->readfds))
    {
      p->nready--;

      state = p->states[i];

      /* Recv bytes from the client */
      n = Recv(client_fd, NULL, buf, BUF_SIZE);

      /* We have received bytes, send for parsing. */
      if (n >= 1)
      {
        store_request(buf, n, state);

        /* The loop that keeps servicing pipelined request */
        do{
          /* First, parse method, URI and version. */
          if(state->method == NULL)
          {
            /* Malformed Request */
            if((error = parse_line(state)) != 0 && error != -1)
            {
              client_error(state, error);
              if (Send(client_fd, NULL, state->response, state->resp_idx)
                  != state->resp_idx)
              {
                rm_client(client_fd, p, "Unable to write to client", i);
                break;
              }
              rm_client(client_fd, p, "HTTP error", i);
              break;
            }

            /* Incomplete request, save and continue to next client */
            if(error == -1) break;
           }

          /* Then, parse headers. */
          if(state->header == NULL && state->method != NULL)
          {
            if((error = parse_headers(state)) != 0)
            {
              client_error(state, error);
              if (Send(client_fd, NULL, state->response, state->resp_idx) !=
                  state->resp_idx)
              {
                rm_client(client_fd, p, "Unable to write to client", i);
                break;
              }
              rm_client(client_fd, p, "HTTP error", i);
              break;
            }
          }

          /* If POST, parse the body */
          if(!strncmp(state->method, "POST", strlen("POST")) &&
             state->body == NULL)
          {
            if((error = parse_body(state)) != 0 && error != -1)
            {
              client_error(state, error);
              if (Send(client_fd, NULL, state->response, state->resp_idx) !=
                  state->resp_idx)
              {
                rm_client(client_fd, p, "Unable to write to client", i);
                break;
              }
              rm_client(client_fd, p, "HTTP error", i);
              break;
            }

            /* Incomplete request, save and continue to next client */
            if(error == -1) break;
          }

          /* If everything has been parsed, service the client */
          if(state->method != NULL && state->header != NULL)
          {
            if ((error = service(state)) != 0)
            {
              client_error(state, error);
              if (Send(client_fd, NULL, state->response, state->resp_idx) !=
                  state->resp_idx)
              {
                rm_client(client_fd, p, "Unable to write to client", i);
                break;
              }
              rm_client(client_fd, p, "HTTP error", i);
              break;
            }

            /* if POST/GET CGI */
            if(state->pipefds > 0)
            {
              if(add_cgi(client_fd, state, p))
              {
                rm_client(client_fd, p, "Too many processes", i);
                break;
              }
            }
            /* Regular GET/HEAD */
            else if (Send(state->servfd, NULL, state->response, state->resp_idx)
                     != state->resp_idx ||
                     Send(state->servfd, NULL, state->body, state->body_size)
                     != state->body_size)
              {
                rm_client(client_fd, p, "Unable to write to server", i);
                break;
              }

            memset(buf,0,BUF_SIZE);
          }

          /* Finished serving one request, reset buffer */
          state->end_idx = resetbuf(state);
          clean_state(state);
          if(!state->conn) rm_client(client_fd, p, "Connection: close", i);
        } while(error == 0 && state->conn);
        continue;
      }

      /* Client sent EOF, close socket. */
      if (n == 0)
      {
        rm_client(client_fd, p, "Client closed connection with EOF", i);
      }

      /* Error with recv */
      if (n == -1)
      {
        rm_client(client_fd, p, "Error reading from client socket", i);
      }
    } // End of client read check

    memset(buf,0,BUF_SIZE);

    if(state->servfd > 0 && FD_ISSET(state->servfd, &p->readfds))
      {
        p->nready--;

        /* Receive bytes from the webserver */
        n = Recv(state->servfd, NULL, buf, BUF_SIZE);

        if (n >= 1)
          {
            store_request(buf, n, state->servst);

            servst = state->servst;

            /* perform the pipelining loop */
            do {

              /* Parse the status line */
              if (servst->status == NULL)
                {
                  if((error = parse_status(servst)) != 0 && error != -1)
                    {
                      printf("parse_status error!!\n");
                      exit(0);
                    }
                }

              /* Incomplete response; save and move on */
              if(error == -1) break;

              /* Parse the headers */
              if(servst->headers == NULL && servst->status != NULL)
                {
                  if((error = parse_headers(servst)) != 0 && error != -1)
                    {
                      printf("parse_status error!!\n");
                      exit(0);
                    }
                }

              if(error == -1) break;

              /* Parse the body */
              if(servst->status != NULL && servst->headers !=NULL)
                {
                  if((error = parse_body(servst)) != 0 && error != -1)
                    {
                      printf("parse_status error!!\n");
                      exit(0);
                    }
                }

              if(error == -1) break;

              /* Everything has been parsed. */
              /* If .f4m file, proceed to save it.*/
              /* Else, just send it to the client. */
              if(state->expecting == REGF4M)
                {
                  parse_f4m(state);
                  servst->expecting = VIDEO;
                }
              else // VIDEO or NOLIST
                {
                  /* Just pass it on to the client */
                  Send(client_fd, NULL, buf, n);
                }

              /* Calculate new throughput here */
            }
          }
      } // End of webserver read check
  } // End of single client loop.
}

/***************************************************************************/
/* @brief Removes a client and its state from the maintained pool, freeing */
/* up resources and cleaning up memory                                     */
/*                                                                         */
/* @param client_fd  The client fd to be removed from the pool             */
/* @param p          The pool from which to be removed                     */
/* @param logmsg     A msg to write to the logfile                         */
/* @param i          The index into the pool                               */
/***************************************************************************/
void rm_client(int client_fd, pool* p, char* logmsg, int i)
{
  /* Sanitize memory */
  fsm* state = p->states[i];
  if(state->context != NULL) SSL_free(state->context);
  delfromfree(state->freebuf, FREE_SIZE);
  free(state);

  close_socket(client_fd);
  FD_CLR(client_fd, &p->masterfds);
  p->clientfd[i] = -1;
}


/************************************************************/
/* @brief  Writes an error message into the state provided. */
/*                                                          */
/* @param state  The state to write to                      */
/* @param error  The error to write to                      */
/************************************************************/
void client_error(fsm* state, int error)
{
  char* response = state->response;
  char body[LOG_SIZE] = {0};
  char* errnum; char* errormsg;

  memset(response,0,BUF_SIZE);

  switch (error)
  {
    case 404:
      errnum    = "404";
      errormsg  = "Not Found";
      break;
    case 411:
      errnum    = "411";
      errormsg  = "Length Required";
      break;
    case 500:
      errnum    = "500";
      errormsg  = "Internal Server Error";
      break;
    case 501:
      errnum    = "501";
      errormsg  = "Not Implemented";
      break;
    case 503:
      errnum    = "503";
      errormsg  = "Service Unavailable";
      break;
    case 505:
      errnum    = "505";
      errormsg  = "HTTP Version Not Supported";
      break;
    case 400:
      errnum    = "400";
      errormsg  = "Bad Request";
  }

  /* Build the HTTP response body */
  sprintf(response, "HTTP/1.1 %s %s\r\n", errnum, errormsg);
  sprintf(response, "%sContent-type: text/html\r\n", response);
  sprintf(response, "%sServer: Liso/1.0\r\n", response);

  sprintf(body, "<html><title>Webserver Error!</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, errormsg);
  sprintf(body, "%s<hr><em>Fadhil's Web Server </em>\r\n", body);

  sprintf(response, "%sConnection: close\r\n",      response);
  sprintf(response, "%sContent-Length: %d\r\n\r\n", response,(int)strlen(body));

  sprintf(response, "%s%s", response, body);

  state->resp_idx += strlen(response);
}

void cleanup(int sig)
{
  int appease_compiler = sig;
  appease_compiler += 2;

  log_close(logfile);

  fprintf(stderr, "\nThank you for flying Liso. See ya!\n");
  exit(1);
}

void
sigchld_handler(int sig)
{
  pid_t pid; int status;
  int appease_compiler = 0;
  appease_compiler += sig;

  while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
  {
    /* Child terminated because of a
     * signal that was not caught. */
    fprintf(stderr, "Child reaped\n");
  }
  return;
}

/************************************************************/
/* @brief Connects to the webserver that has the videos.    */
/* @param state - The state of the client behind the proxy. */
/************************************************************/
void connect_server(fsm* state)
{
  int status, sock;
  struct addrinfo hints; struct sockaddr_in fake;
  struct addrinfo *servinfo; //will point to the results

  hints.ai_family = AF_UNSPEC;  //don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; //TCP stream sockets
  hints.ai_flags = AI_PASSIVE; //fill in my IP for me

  /* Bind the fake IP to the socket */
  fake.sin_family      = AF_UNSPEC;
  fake.sin_addr.s_addr = inet_addr(fake_ip);
  fake.sin_port        = 0;

  if ((status = getaddrinfo(www_ip, "80", &hints, &servinfo)) != 0)
    {
      fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
      return EXIT_FAILURE;
    }

  if((sock = socket(servinfo->ai_family, servinfo->ai_socktype,
                    servinfo->ai_protocol)) == -1)
    {
      fprintf(stderr, "Socket failed");
      return EXIT_FAILURE;
    }

  /* Bind this socket to the fake-ip with an ephemeral port */
  bind(sock, (struct sockaddr *) &fake, sizeof(fake));

  if (connect (sock, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
      fprintf(stderr, "Connect");
      return EXIT_FAILURE;
    }

  /* We now have a unique connection established for this client */
  state->servfd = sock;
  state->servst = calloc(sizeof(struct serv_rep), 1);
  strncpy(state->serv_ip, www_ip, INET_ADDRSTRLEN);

  freeaddrinfo(servinfo);
}
