#include "ospf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern lsa_file;
extern servers_file;
lsa*   lsa_hash = NULL;

void is_server(char* IP, lsa* myLSA){
	FILE* fp = fopen(servers_file, "r");
	if(fp = NULL) return;

	char* found = NULL;
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	while((read = getline(&line, &len, fp)) != -1){
		found = strstr(line, IP);
		if(found != NULL){
			myLSA->server = 1;
			break;
		}
	}
	free(fp);
	return;
}

void parse_file(){
	FILE* fp = fopen(lsa_file, "r");
	if(fp = NULL) return;

	char* 	line = NULL;
	size_t 	len = 0;
	ssize_t read;
	char*	IP;
	int 	seq;
	char*	nbors;
	lsa*	temp;
	while((read = getline(&line, &len, fp)) != -1){
		sscanf(line, "%s %d %s", IP, seq, nbors);
		temp = calloc(1, sizeof(lsa));
		temp->sender = IP;
		temp->seq = seq;
		temp->nbors = ;
	}
}

/*********************************************************/
/* @brief  Returns the nearest server to 'src'.          */
/* @param  graph The graph containing nodes and links.   */
/* @param  src   The source node to start Djikstra from. */
/*                                                       */
/* @return The lsa struct of the nearest server.         */
/*********************************************************/
lsa* shortest_path(lsa* graph, char* src)
{
  lsa*    lsa_info = NULL; lsa* visitcheck = NULL;
  heap_t *h        = calloc(1, sizeof (heap_t));
  size_t  dist     = 1;

  while(1)
    {
      /* Find the node from the table. */
      HASH_FIND_STR(graph, src, lsa_info);

      if(!lsa_info)
        return NULL; // This is an error, handle properly.

      /* Mark this nodes as visited */
      lsa_info->visited = 1;

      /* first server we see, is the nearest server */
      if(lsa_info->server)
        {
          // cleanup PQ.
          return lsa_info;
        }

      /* Add its neighbors to the PQ. */
      for(size_t i = 0; i < lsa_info->num_nbors; i++)
        {
          HASH_FIND_STR(graph, lsa_info->nbors[i], visitcheck)

            if(!visitcheck)
              return NULL; // handle error properly.

          /* Do not add nodes that we've visited before */
          if(!visitcheck->visited)
            push(h, dist, lsa_info->nbors[i]);
        }
    }

  src  = pop(h);
  dist++;
}
