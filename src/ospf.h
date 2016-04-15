#ifndef OSPF_H
#define OSPF_H


typedef struct lsa{
  char 		sender[40];
  int 		seq;
  int 		server; 
  int 		num_nbors;
  char** 	nbors;
  UT_hash_handle hh;
} lsa;

#endif
