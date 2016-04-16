#include "nsd.h"

/* Globals */
bool  rr;
char* log_file;
char* servers_file;
char* lsa_file;

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
  if ((listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1)
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

  /* Begin a passive server side listen. */
  if (listen(listen_fd, 5))
    {
      close(listen_fd);
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}

void usage()
{
  printf("Usage: ./nameserver [-r] <log> <ip> <port> <servers> <LSAs> \n");
  exit(1);
}
