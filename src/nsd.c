#include "nsd.h"

/* Globals */
bool  rr;
char* log_file;
char* servers_file;
char* lsa_file;
lsa*  lsa_hash = NULL;

int main(int argc, char* argv[])
{
  char                 *ip;
  int                  listen_fd, port;
  struct sockaddr_in   serv_addr, hints;

  if (argc == 6)
    {
      rr = false;
      log_file     = argv[1];
      ip           = argv[2];
      port         = atoi(argv[3]);
      servers_file = argv[4];
      lsa_file     = argv[5];
    }
  else if (argc == 7)
    {
      rr = true;
      log_file     = argv[2];
      ip           = argv[3];
      port         = atoi(argv[4]);
      servers_file = argv[5];
      lsa_file     = argv[6];
    }
  else
    usage();

  signal(SIGPIPE, SIG_IGN);

  fprintf(stdout, "--------Welcome to the fabulous DNS server!------\n");

  /* Create a socket for comms */
  if ((listen_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
      return EXIT_FAILURE;
    }

  /* Bind to the DNS server IP/port */
  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family      = AF_UNSPEC;
  serv_addr.sin_port        = htons(port);
  serv_addr.sin_addr.s_addr = inet_addr(ip);

  /* Set sockopt so that ports can be resued */
  int enable = -1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enable,
                 sizeof(int)) == -1)
    {
      close(listen_fd);
      return EXIT_FAILURE;
    }

  /* servers bind sockets to ports---notify the OS they accept connections */
  if (bind(listen_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)))
    {
      close(listen_fd);
      return EXIT_FAILURE;
    }

  /* Prepare for select loop */
  int nfds;
  fd_set readfds;

  while(1)
    {
      FD_ZERO(&readfds);
      FD_SET(listen_fd, &readfds);

      nfds = select(listen_fd, &readfds, NULL, NULL, NULL);

      if (FD_ISSET(listen_fd, &readfds))
        process_inbound_udp(listen_fd);
    }

  return EXIT_SUCCESS;
}

/******************************************************/
/* @brief Processes an inboud UDP datagram containing */
/*        a DNS message.                              */
/*                                                    */
/* @param sock  The socket to read messages from.     */
/******************************************************/
void process_inbound_udp(int sock)
{
  #define              BUFLEN         512
  uint8_t              buf[BUFLEN]  = {0};
  struct  sockaddr_in  from         = {0};

  recvfrom(sock, buf, BUFLEN, (struct sockaddr *) &from, sizeof(from));


}

void usage()
{
  printf("Usage: ./nameserver [-r] <log> <ip> <port> <servers> <LSAs> \n");
  exit(1);
}
