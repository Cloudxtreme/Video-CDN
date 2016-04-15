#ifndef OSPF_H
#define OSPF_H


typedef struct lsa{
  char 		sender[40];
  int 		seq;
  int 		server;
  int     visited;  // for djikstra
  size_t 	num_nbors;
  char** 	nbors;
  UT_hash_handle hh;
} lsa;

void shortest_path(lsa* graph, char* src, char* dest);

#endif
