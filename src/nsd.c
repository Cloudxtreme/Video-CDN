#include "nsd.h"
#include "mydns.h"

/* Globals */
bool   rr;
size_t rrcount = 0;
size_t numsrvs = 0;

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

  /* Count the number of servers */
  numsrvs = num_server();

  /* Parse the LSAs */
  parse_file();

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
  size_t               n            = 0;
  question*            query        = NULL;

  n = recvfrom(sock, buf, BUFLEN, (struct sockaddr *) &from, sizeof(from));

  dns_message* msg   = parse_message(buf);
  query              = msg->questions[0];

  uint8_t iphex[4] = {0};

  if(rr)
    {
      lsa* current  = NULL;
      lsa* temp     = NULL;
      size_t cntdwn = rrcount;

      HASH_ITER(hh, lsa_hash, current, temp)
        {
          if(current->server)
            {
              if(!cntdwn)
                {
                  /* Select this server */
                  gen_RDATA(current->sender, iphex);
                  rrcount = (rrcount + 1) % numsrvs;
                  break;
                }
              cntdwn--;
            }
        }
    }
  else
    {
      lsa* nearest = shortest_path(lsa_hash, (char *) query->NAME);

      /* Select this server */
      gen_RDATA(nearest->sender, iphex);
    }

  //@assert strlen(iphex) > 0

  answer* response =
    gen_answer(query->QNAME, query->name_size + 1, iphex);

    byte_buf* msg2send = gen_message(binary2int(msg->ID, 2), 1, 0, 1,
                                     0, 0, 0, 0
                                     1, 1
                                     msg->questions, &response);

    sendto(sock, msg2send->buf, msg2send->pos, 0,
           &from, sizeof(from));
}

void usage()
{
  printf("Usage: ./nameserver [-r] <log> <ip> <port> <servers> <LSAs> \n");
  exit(1);
}
