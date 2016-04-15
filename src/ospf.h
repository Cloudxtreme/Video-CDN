#ifndef OSPF_H
#define OSPF_H

#define MAX_IP_SIZE		40

typedef struct lsa{
  char 		sender[MAX_IP_SIZE];
  int 		seq;
  int 		server; 
  int 		num_nbors;
  char** 	nbors;
  UT_hash_handle hh;
} lsa;

#endif
