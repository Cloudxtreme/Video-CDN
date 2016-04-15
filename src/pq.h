#ifndef PQ_H
#define PQ_H

#include <stdio.h>
#include <stdlib.h>
#include "pq.h"

typedef struct {
  int priority;
  char *data;
} node_t;

typedef struct {
  node_t *nodes;
  int len;
  int size;
} heap_t;

void push (heap_t *h, int priority, char *data);
char *pop (heap_t *h);
void clean(heap_t *h);

#endif
