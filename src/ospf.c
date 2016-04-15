#include "ospf.h"

extern

void shortest_path(lsa* graph, char* src)
{
  lsa* lsa_info = NULL;

  /*

    Pseudocode:
    1. Find the 'src' node in the hash table.
    2. Add its neighbors to the priority queue.
    3. char* nearest = Pop from the priority queue.
    4. Retrieve nearest from the hash table.
    5. if nearest == server, return this guy.
       else repeat from 2.

   */

  /* Find the source node from the table. */
  HASH_FIND_STR(graph, src, lsa_info);



}
