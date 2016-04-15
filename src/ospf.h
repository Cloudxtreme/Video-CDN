#ifndef OSPF_H
#define OSPF_H

typedef struct santa{
  char           name[40];
  UT_hash_handle hh;
} santa;

typedef struct lsa{
  char           sender[40];
  int            seq;
  santa*         nbors;
  UT_hash_handle hh;
} lsa;

void shortest_path(lsa* graph, char* src, char* dest);

#endif
