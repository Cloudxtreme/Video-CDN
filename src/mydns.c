#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "mydns.h"

/* Globals */
bool  rr;
char* log_file;
char* servers_file;
char* lsa_file;

int main(int argc, char* argv[])
{
  char *ip, *port;
  int listen_fd;

  if (argc == 6)
    {
      rr = false;
      log_file     = argv[1];
      ip           = argv[2];
      port         = argv[3];
      servers_file = argv[4];
      lsa_file     = argv[5];
    }
  else if (argc == 7)
    {
      rr = true;
      log_file     = argv[2];
      ip           = argv[3];
      port         = argv[4];
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

  /* Listen on the DNS server IP/port */
}

void usage()
{
  printf("Usage: %s [-r] <log> <ip> <port> <servers> <LSAs> \n", argv[0]);
  exit(1);
}
