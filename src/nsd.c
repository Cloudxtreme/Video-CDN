#include "nsd.h"


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
  struct sockaddr_in   serv_addr;

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

  /*
  lsa* current;
  lsa* temp;

  HASH_ITER(hh, lsa_hash, current, temp)
    {
      printf("Node: %s\n", current->sender);
      for(int i = 0; i < (int)current->num_nbors; i++){
        printf("Neighbor %d: %s\n", i, current->nbors[i]);
      }
    }
  */

  signal(SIGPIPE, SIG_IGN);

  fprintf(stdout, "--------Welcome to the fabulous DNS server!------\n");

  /* Create a socket for comms */
  if ((listen_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      return EXIT_FAILURE;
    }

  /* Bind to the DNS server IP/port */
  bzero(&serv_addr, sizeof(serv_addr));
  serv_addr.sin_family      = AF_INET;
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
      printf("%s\n", strerror(errno));
      return EXIT_FAILURE;
    }

  /* Prepare for select loop */
  fd_set readfds;

  while(1)
    {
      FD_ZERO(&readfds);
      FD_SET(listen_fd, &readfds);

      select(listen_fd + 1, &readfds, NULL, NULL, NULL);

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
  socklen_t            fromlen      = sizeof(from);
  question*            query        = NULL;

  ssize_t n = recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

  (void) n;

  char* fromip = inet_ntoa(from.sin_addr);

  printf("msg from : %s \n", fromip);

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

                  log_dns(inet_ntoa(from.sin_addr), current->sender, log_file);

                  break;
                }
              cntdwn--;
            }
        }
    }
  else
    {
      lsa* nearest = shortest_path(lsa_hash, fromip);

      log_dns(inet_ntoa(from.sin_addr), nearest->sender, log_file);

      /* Select this server */
      gen_RDATA(nearest->sender, iphex);
    }

  //@assert strlen(iphex) > 0

  byte_buf* qname2send = gen_QNAME((char *) query->NAME, query->name_size);

  question* q2send     = gen_question(qname2send->buf, qname2send->pos + 2);

  question** dumquery  = calloc(1, sizeof(question*));
  dumquery[0]          = q2send;

  answer* response =
      gen_answer(qname2send->buf, qname2send->pos + 2, iphex);

  answer** dumresponse = calloc(1, sizeof(answer*));
  dumresponse[0] = response;


  struct byte_buf* msg2send =
      gen_message(binary2int(msg->ID, 2), 1, 0, 1,
                  0, 0, 0, 0,
                  1, 1,
                  dumquery, dumresponse);

  sendto(sock, msg2send->buf, msg2send->pos, 0,
         (struct sockaddr *) &from, sizeof(from));

  free_dns(msg);
  delete_bytebuf(msg2send);
}

void usage()
{
  printf("Usage: ./nameserver [-r] <log> <ip> <port> <servers> <LSAs> \n");
  exit(1);
}
