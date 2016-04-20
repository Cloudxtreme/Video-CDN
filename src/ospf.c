#include "ospf.h"

//valgrind --db-attach=yes --leak-check=yes --tool=memcheck --num-callers=16 --leak-resolution=high
//./nameserver logNSD.txt 5.0.0.1 38296 ../bitrate-project-starter/topos/topo1/topo1.server ../bitrate-project-starter/topos/topo1/topo1.lsa
//./proxy log.txt 0.125 64589 1.0.0.1 5.0.0.1 38666

extern char* lsa_file;
extern char* servers_file;
extern lsa*  lsa_hash;

int get_comma_count(char *nbors){
	int comma_count = 0;
	char* line 		= strstr(nbors, ",");
	while(line != NULL){
		comma_count++;
		line = strstr(line+1, ",");
	}
	return comma_count;
}

size_t num_server()
{
  FILE* fp   = fopen(servers_file, "r");
  if(fp == NULL) return 0;
  char* line = NULL;
  size_t len = 0;
  size_t num = 0;
  ssize_t read;

  if(fp == NULL) return 0;

  while((read = getline(&line, &len, fp)) != -1)
    {
      num++;
    }

  free(line);
  fclose(fp);
  return num;
}

void is_server(char* IP, lsa* myLSA){
	FILE* fp = fopen(servers_file, "r");
	if(fp == NULL) return;

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
	free(line);
	fclose(fp);
	return;
}

void parse_nbors(lsa* myLSA, char *nbors){
	int comma_count = get_comma_count(nbors);
	int token_count = 0;
	char* token;

	myLSA->num_nbors = comma_count + 1;
	myLSA->nbors = calloc(1, myLSA->num_nbors * sizeof(char*));
	for(int i = 0; i <= comma_count; i++){
		myLSA->nbors[i] = calloc(1, MAX_IP_SIZE + 1);
	}

	token = strtok(nbors, ",");
	while(token){
		strcpy(myLSA->nbors[token_count], token);
		token = strtok(NULL, ",");
		token_count++;
	}
	return;
}

void parse_file(){
	FILE* fp = fopen(lsa_file, "r");
	if(fp == NULL) return;

	char* 	line = NULL;
	size_t 	len = 0;
	ssize_t read;
	char	IP[MAX_IP_SIZE + 1];
	memset(IP, 0, MAX_IP_SIZE + 1);
	int 	seq;
	char*	nbors;
	lsa*	temp;
	lsa*	find;

	while((read = getline(&line, &len, fp)) != -1){
		nbors = malloc(MAX_IP_SIZE * get_comma_count(line));
		sscanf(line, "%s %d %s", IP, &seq, nbors);
		temp = calloc(1, sizeof(lsa));
		strcpy(temp->sender, IP);
		temp->seq = seq;
		is_server(IP, temp);
		parse_nbors(temp, nbors);
		HASH_FIND_STR(lsa_hash, IP, find);
		if(find == NULL){
			HASH_ADD_STR(lsa_hash, sender, temp);
		} else {
			if(find->seq < temp->seq){
				for(int i = 0; i < (int)find->num_nbors; i++){
					free(find->nbors[i]);
				}
				free(find->nbors);
				find->nbors = temp->nbors;
				find->num_nbors = temp->num_nbors;
				find->seq = temp->seq;
			}
		}
		free(nbors);
	}
	free(line);
	fclose(fp);
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

      if(!lsa_info) return NULL; // This is an error, handle properly.

      /* Mark this nodes as visited */
      lsa_info->visited = 1;

      /* first server we see, is the nearest server */
      if(lsa_info->server)
        {
          // cleanup PQ.

          lsa* current;
          lsa* temp;

          HASH_ITER(hh, graph, current, temp)
            {
              current->visited = 0;
            }

          return lsa_info;
        }

      /* Add its neighbors to the PQ. */
      for(size_t i = 0; i < lsa_info->num_nbors; i++)
        {
          HASH_FIND_STR(graph, lsa_info->nbors[i], visitcheck);

          if(!visitcheck) continue; // could be DNS, continue.

          /* Do not add nodes that we've visited before */
          if(!visitcheck->visited)
            push(h, dist, lsa_info->nbors[i]);
        }

      src  = pop(h);
      dist++;
    }

  return NULL;
}
