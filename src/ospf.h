#ifndef OSPF_H
#define OSPF_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pq.h"
#include "uthash.h"

#define MAX_IP_SIZE		40

/* Stores relevant info about the IP and its neighbors */
typedef struct lsa{
  char 		sender[MAX_IP_SIZE];
  int 		seq;
  int 		server;
  int     visited;  // for djikstra
  size_t 	num_nbors;
  char** 	nbors;
  UT_hash_handle hh;
} lsa;

lsa* shortest_path(lsa* graph, char* src);
size_t num_server();
void   parse_file();

#endif
