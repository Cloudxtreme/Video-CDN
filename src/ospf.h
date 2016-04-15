#ifndef OSPF_H
#define OSPF_H

#define MAX_IP_SIZE		40

typedef struct lsa{
  char 		sender[MAX_IP_SIZE];
  int 		seq;
  int 		server;
  int     visited;  // for djikstra
  size_t 	num_nbors;
  char** 	nbors;
  UT_hash_handle hh;
} lsa;

void shortest_path(lsa* graph, char* src, char* dest);

#endif
