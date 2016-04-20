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

#include "proxy.h"
#include "logger.h"
#include "engine.h"
#include "parse.h"
#include "uthash.h"
#include "mydns.h"

/** Global vars **/
FILE* logfile;
float alpha;

short listen_port;
char* fake_ip;
char* dns_ip;
short dns_port;
char* www_ip;

bool  dns;
int   dns_sock;

/* Linkd list of bitrates */
struct bitrate *all_bitrates = NULL;
unsigned long long int global_best;

/** Prototypes **/

int  close_socket(int sock);
void init_pool(int listenfd, int dns_sock, pool *p);
void add_client(int client_fd, pool *p);
void check_clients(pool *p, int dns_sock);
void cleanup(int sig);
void sigchld_handler(int sig);
int connect_server(fsm* state, char* webip, in_addr_t dnsip);

/** Definitions **/

int resolve(char *node, char *service,
            const struct addrinfo *hints, struct addrinfo **res)
{
  (void) hints;
  (void) res;
  (void) service;

  struct sockaddr_in dns_addr;

  bzero(&dns_addr, sizeof(dns_addr));
  dns_addr.sin_family  = AF_UNSPEC;
  dns_addr.sin_port    = htons(dns_port);
  dns_addr.sin_addr.s_addr = inet_addr(dns_ip);

  byte_buf* QNAME_bb = gen_QNAME(node, strlen(node));
  question* query    = gen_question(QNAME_bb->buf, strlen((char *) QNAME_bb->buf) + 1);

  question** dumquery = calloc(1, sizeof(question*));
  dumquery[0]         = query;

  srand(time(NULL));

  struct byte_buf* msg2send = gen_message(rand(), 0, 0, 0, 0,
                                   0, 0, 0,
                                   1, 0,
                                   dumquery, NULL);

  sendto(dns_sock, msg2send->buf, msg2send->pos, 0,
         (struct sockaddr *)&dns_addr, sizeof(dns_addr));

  free(query->NAME);
  free(query);
  delete_bytebuf(msg2send);
  delete_bytebuf(QNAME_bb);
  return 0;
}

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

  if (argc == 7)
    dns = true;
  else
    dns = false;

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
  if(argc == 8)
    www_ip          = argv[7];

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
  serv_addr.sin_family        = AF_UNSPEC;
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

  /* Create a UDP socket for dns comms */
  if ((dns_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      return EXIT_FAILURE;
    }

  /***************************************************/
  /* Use this to talk to the DNS server:             */
  /*                                                 */
  /* struct sockaddr_in dnsaddr = {0};               */
  /* dnsaddr.sin_family         = AF_INET;           */
  /* dnsaddr.sin_addr.s_addr    = inet_addr(dns_ip); */
  /* dnsaddr.sin_port           = htons(dns_port);   */
  /***************************************************/

  /* Initialize our pool of fds */
  init_pool(listen_fd, dns_sock, pool);

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
      check_clients(pool, dns_sock);
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
 * @param dns_sock The DNS server socket.
 * @param p        The pool struct to initialize.
 */
void init_pool(int listenfd, int dns_sock, pool *p)
{
  p->maxi = -1;

  memset(p->clientfd, -1, FD_SETSIZE*sizeof(int)); // No clients at the moment.
  memset(p->dns,       false,  FD_SETSIZE*sizeof(bool)); // No dns yet.
  memset(p->states,    0, FD_SETSIZE*sizeof(fsm*)); // NULL out the fsms.

  /* Initailly, listenfd and dns_sock are the only members of the read set */
  p->maxfd = dns_sock;
  FD_ZERO(&p->masterfds);
  FD_SET(listenfd, &p->masterfds);
  FD_SET(dns_sock, &p->masterfds);
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
  bzero(state->serv_ip, INET_ADDRSTRLEN);

  state->avg_tput   = 1000;

  if(!dns)
    connect_server(state, www_ip, 0);
  else
    resolve("video.cs.cmu.edu", "8080", NULL, NULL);

  memset(state->freebuf, 0, FREE_SIZE*sizeof(char*));

  for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
    {
      if(p->clientfd[i] < 0) {   /* Found one free slot */
        p->clientfd[i] = client_fd;

        /* Add the descriptor to the master set */
        FD_SET(client_fd, &p->masterfds);
        FD_SET(state->servfd, &p->masterfds);

        /* Add fsm to pool */
        p->states[i] = state;

        /* Expecting DNS response */
        p->dns[i]    = true;

        /* Update max descriptor and max index */
        if (client_fd > p->maxfd)
          p->maxfd = client_fd;

        if(state->servfd > p->maxfd)
          p->maxfd = state->servfd;

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
/* @param dns_sock The DNS server socket                             */
/*********************************************************************/
void check_clients(pool *p, int dns_sock)
{
  int i, client_fd, n, error;
  fsm* state;
  char buf[BUF_SIZE] = {0};
  struct serv_rep* servst;

  memset(buf,0,BUF_SIZE);

  /* Iterate through all clients, and read their data */
  for(i = 0; (i <= p->maxi) && (p->nready > 0); i++)
    {
      if((client_fd = p->clientfd[i]) <= 0)
        continue;

      state  = p->states[i];

      /* Are we using DNS ? */
      if(dns)
        {
          /* Check if there is a 'requested' DNS response for this socket */
          if(FD_ISSET(dns_sock, &p->readfds) && p->dns[i])
            {
              #define              BUFLEN         512
              uint8_t              buf[BUFLEN]  = {0};
              struct  sockaddr_in  from         = {0};
              socklen_t            fromlen      = sizeof(from);
              //size_t               n            = 0;
              answer*              reply        = NULL;

              recvfrom(dns_sock, buf, BUFLEN, 0,
                       (struct sockaddr *) &from, &fromlen);

              dns_message* msg = parse_message(buf);
              reply            = msg->answers[0];

              in_addr_t ip     = (in_addr_t) binary2int(reply->RDATA, 4);

              connect_server(state, NULL, ip);

              /* Free memory */
              free_dns(msg);

              /* Not requesting DNS anymore...*/
              p->dns[i] = false;
            }
          /* DNS requested but never came...*/
          else if(p->dns[i])
            continue;

          /* DNS requested and received, proceed. */
        }

      /* If a descriptor is ready to be read, read a line from it */
      if (client_fd > 0 && FD_ISSET(client_fd, &p->readfds))
        {
          p->nready--;

          state = p->states[i];

          /* Recv bytes from the client */
          n = Recv(client_fd, buf, BUF_SIZE);

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
                        if (Send(client_fd, state->response, state->resp_idx)
                            != state->resp_idx)
                          {
                            rm_client(client_fd, p, i);
                            break;
                          }
                        rm_client(client_fd, p, i);
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
                        if (Send(client_fd, state->response, state->resp_idx) !=
                            state->resp_idx)
                          {
                            rm_client(client_fd, p, i);
                            break;
                          }
                        rm_client(client_fd, p, i);
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
                        if (Send(client_fd, state->response, state->resp_idx) !=
                            state->resp_idx)
                          {
                            rm_client(client_fd, p, i);
                            break;
                          }
                        rm_client(client_fd, p, i);
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
                        if (Send(client_fd, state->response, state->resp_idx) !=
                            state->resp_idx)
                          {
                            rm_client(client_fd, p, i);
                            break;
                          }
                        rm_client(client_fd, p, i);
                        break;
                      }

                    /* Regular GET/HEAD */
                    else if (Send(state->servfd, state->response, state->resp_idx)
                             != state->resp_idx ||
                             Send(state->servfd, state->body, state->body_size)
                             != state->body_size)
                      {
                        rm_client(client_fd, p, i);
                        break;
                      }

                    /* Clock the start time */
                    clock_gettime(CLOCK_MONOTONIC, &state->start);

                    memset(buf,0,BUF_SIZE);
                  }

                /* Finished serving one request, reset buffer */
                state->end_idx = resetbuf(state);
                clean_state(state);
                if(!state->conn) rm_client(client_fd, p, i);
              } while(error == 0 && state->conn);
              continue;
            }

          /* Client sent EOF, close socket. */
          if (n == 0)
            {
              rm_client(client_fd, p, i);
            }

          /* Error with recv */
          if (n == -1)
            {
              rm_client(client_fd, p, i);
            }
        } // End of client read check

      memset(buf,0,BUF_SIZE);

      /* Check if the webserver has sent something to be relayed to this client */
      if(state->servfd > 0 && FD_ISSET(state->servfd, &p->readfds))
        {
          p->nready--;

          /* Receive bytes from the webserver */
          n = Recv(state->servfd, buf, BUF_SIZE);

          clock_gettime(CLOCK_MONOTONIC, &state->end);

          if (n >= 1)
            {
              servst = state->servst;

              /* perform the pipelining loop */
              do {

                /* If not, REGF4M, just send it away */
                if(servst->expecting != REGF4M)
                  {
                    /* Just pass it on to the client */
                    Send(client_fd, buf, n);

                    state->body_size = n;

                    /* Calculate new throughput here */
                    calculate_bitrate(state);

                    break;
                  }

                /* Is this the body or the status/headers? */
                if (servst->headers == NULL)
                  /*  Store the status msg */
                  store_request_serv(buf, n, state->servst);

                /* Parse the headers */
                if(servst->headers == NULL)
                  {
                    if((error = parse_headers_serv(state)) != 0 && error != -1)
                      {
                        printf("parse_status error!!\n");
                        exit(0);
                      }

                    /* Remove headers and store only body data from buf */
                    if(error == 0)
                      {
                        char* CRLF = memmem(buf, n, "\r\n\r\n", strlen("\r\n\r\n"));
                        n = resetbuf_serv(buf, CRLF+4 - buf, n);
                      }

                    /* Incomplete headers, save and work on this later */
                    if(error == -1) break;
                  }

                /* Parse the body */
                if(servst->headers != NULL && servst->body_idx < servst->body_size)
                  {
                    error = parse_body_serv(servst, buf, n);

                    /* More body data has to be sent, save for later */
                    if(error == -1) break;
                  }

                /* Everything has been parsed. */
                /* If .f4m file, proceed to save it.*/
                parse_f4m(state);
                servst->expecting = NOLIST;

                /* Cleanup servstate */
                free(servst->body);
                servst->body = NULL;

                if(error == 0)
                  break;

                n = error;
              } while(error > 0);
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
void rm_client(int client_fd, pool* p, int i)
{
  /* Sanitize memory */
  fsm* state = p->states[i];
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
int connect_server(fsm* state, char* webip, in_addr_t dnsip)
{
  int status, sock;
  struct addrinfo hints; struct sockaddr_in fake;
  struct addrinfo *servinfo; //will point to the results

  memset(&hints, 0, sizeof (hints));
  hints.ai_family = AF_UNSPEC;     //don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; //TCP stream sockets
  hints.ai_flags = AI_PASSIVE;     //fill in my IP for me

  /* Bind the fake IP to the socket */

  memset(&fake, 0, sizeof(fake));
  fake.sin_family      = AF_UNSPEC;
  fake.sin_addr.s_addr = inet_addr(fake_ip);
  fake.sin_port        = 0;

  /* Connect to the www_ip. */
  if(webip)
    {
      if ((status = getaddrinfo(webip, "8080", &hints, &servinfo)) != 0)
        {
          fprintf(stderr, "getaddrinfo error: %s \n", gai_strerror(status));
          return EXIT_FAILURE;
        }
    }
  /* connect to the ip retrieved from the dns request */
  else
    {
      struct sockaddr_in serv = {0};

      serv.sin_family      = AF_INET;
      serv.sin_port        = 8080;
      serv.sin_addr.s_addr = htonl(dnsip);

      servinfo             = (struct addrinfo *)&serv;
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

  if(webip)
    strncpy(state->serv_ip, webip, INET_ADDRSTRLEN);
  else
    {
      struct sockaddr_in* s = (struct sockaddr_in *) servinfo;
      strncpy(state->serv_ip, inet_ntoa(s->sin_addr), INET_ADDRSTRLEN);
    }

  if(webip)
    freeaddrinfo(servinfo);

  return EXIT_SUCCESS;
}
