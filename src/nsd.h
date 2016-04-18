#ifndef NSD_H
#define NSD_H

#include <time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "uthash.h"
#include "ospf.h"
#include "mydns.h"
#include "logger.h"

void usage();
void process_inbound_udp(int sock);

#endif
