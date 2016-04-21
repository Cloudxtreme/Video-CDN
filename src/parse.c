#include "parse.h"
#include "logger.h"

extern FILE* logfile;
extern struct bitrate *all_bitrates;
extern unsigned long long int global_best;
extern unsigned long long global_smallest;

int smallest_bitrate(struct bitrate* b)
{
  int s = b->bitrate;
  struct bitrate* current;
  struct bitrate* tmp;

  HASH_ITER(hh, b, current, tmp)
    {
      if(current->bitrate < s)
        s = current->bitrate;
    }

  return s;
}

/*********************************************************/
/* @brief Parse a .f4m file to obtain bitrates.          */
/* @param state - state of the client to store bitrates. */
/* @param f4m   - The .f4m file.                         */
/*********************************************************/
void parse_f4m(fsm* state)
{
  struct serv_rep* servst = state->servst;
  char *buf = servst->body, *needle1 = NULL, *needle2 = NULL;
  char number[200] = {0};
  size_t len = 0;
  int bitrate_f4m = 0;
  struct bitrate* rate = NULL;

  while((needle1 = strstr(buf, "bitrate=")) != NULL)
    {
      needle2 = strstr(needle1 + strlen("bitrate=") + 1, "\"");
      len     = needle2 - (needle1 + strlen("bitrate=") + 1);

      bzero(number, 200);
      strncpy(number, needle1 + strlen("bitrate=") + 1, len);

      bitrate_f4m = atoi(number);

      rate          = calloc(sizeof(struct bitrate), 1);
      rate->bitrate = bitrate_f4m;
      HASH_ADD_INT(all_bitrates, bitrate, rate);

      buf = needle2;
    }

  global_best = smallest_bitrate(all_bitrates);
  global_smallest = global_best;

}

/* Returns a substring of the given string from [start,end). */
void getSubstring(char *dest, char *src, int start, int end){
    strncpy(dest, src + start, end - start);
    strncat(dest, "\0", 1);
}

/* @brief Calculates the throughput and updates the average throughput and 
          bitrate.
 * @param state: Struct containing relevant info
 */
void calculate_bitrate(fsm* state){

  /* Not a chunk, just an html page or something */
  if(strlen(state->lastchunk) == 0)
    return;

  size_t    size         = state->body_size;
  struct timespec *start = &(state->start);
  struct timespec *end   = &(state->end);
  unsigned long long int    bitrate;
  unsigned long long int    current_best = 0;
  unsigned long long int    smallest = 1000000000;

  unsigned long long int start_time =
    1000000000 * (start->tv_sec) + (start->tv_nsec);
  unsigned long long int end_time =
    1000000000 * (end->tv_sec) + (end->tv_nsec);
  unsigned int long long elapsed = end_time - start_time;
  // elapsed is in nanoseconds.

  unsigned long long throughput = (size * 8 * 1000000) / elapsed; /* kilobits per second */

  struct bitrate* current = NULL;
  struct bitrate* tmp     = NULL;
  state->avg_tput = (alpha * throughput) + (1 - alpha)*(state->avg_tput);

  HASH_ITER(hh, all_bitrates, current, tmp) {
    /* This code loops through all struct bitrates */
    /* All bitrates are in units of Kbps */
    bitrate = current->bitrate;
    if((bitrate * 1.5) < state->avg_tput)
      {
        if(bitrate > current_best)
          current_best = bitrate;
      }

    if(bitrate < smallest)
      smallest = bitrate;
  }

  if(current_best == 0)
    state->current_best = global_best;
  else
    state->current_best = current_best;

  global_best = state->current_best;

  /***********************************************************************/
  /* printf("Throughput is :%lld \n", throughput);                       */
  /* printf("Elapsed time is : %f  \n", ((float) elapsed)/1000000000.0); */
  /* printf("Size is : %zu \n", size);                                   */
  /* printf("\n");                                                       */
  /***********************************************************************/

  log_state(state, logfile, throughput, state->lastchunk, elapsed);
  //  printf("Current best: %lld \n", state->current_best);
}

/* @brief Copies some relevant information into an easier to use struct
 * @param my_req: Destination struct
 * @param client: Source struct
 */
void copy_info(client_req *my_req, struct state *client){
  memcpy(my_req->req_type, client->method, strlen(client->method));
  memcpy(my_req->URI, client->uri, strlen(client->uri));
  memcpy(my_req->version, client->version, strlen(client->version));
  memcpy(my_req->file, client->uri, strlen(client->uri));
  my_req->bitrate = client->current_best; //Change later
}

/* @brief Stores the file location (excluding the file) in the struct
 * @param my_req: Struct containing relevant info
 */
void parse_URI(client_req *my_req){
  int  last_pos = 0;
  char *last_slash = strstr(my_req->URI, "/");
  char temp[BUF_SHORT];
  memset(temp, 0, BUF_SHORT);

  while(last_slash){
    memset(temp, 0, BUF_SHORT);
    memcpy(temp, last_slash, strlen(last_slash));

    if((strstr(last_slash + 1, "/")) == NULL)
      {
	last_pos = (last_slash - (my_req->URI)) + 1;
	break;
      }

    last_slash = strstr(last_slash + 1, "/");
  }

  bzero(my_req->file, 0);
  memcpy(my_req->file, temp + 1, strlen(temp) - 1);
  memset(temp, 0, BUF_SHORT);
  getSubstring(temp, my_req->URI, 0, last_pos);
  memcpy(my_req->path, temp, strlen(temp));
}

/* @brief Retrieves the segment and fragment number.
 * @param my_req: Struct containing relevant info
 */
void parse_file(client_req *my_req){
  char str_bitrate[BUF_SHORT];
  char str_seq_num[BUF_SHORT];
  char str_frag_num[BUF_SHORT];

  int start_pos = 0;
  int end_pos = 0;

  //First, the bitrate. Useless.
  while(isdigit((my_req->file)[end_pos])){
    end_pos++;
  }
  getSubstring(str_bitrate, my_req->file, start_pos, end_pos);

  //Skip the "seg" string
  while(!isdigit((my_req->file)[end_pos])){
    end_pos++;
  }
  start_pos = end_pos;

  //Get the "seq" number
  while(isdigit((my_req->file)[end_pos])){
    end_pos++;
  }
  getSubstring(str_seq_num, my_req->file, start_pos, end_pos);

  //Skip the "frag" string
  while(!isdigit((my_req->file)[end_pos])){
    end_pos++;
  }
  start_pos = end_pos;

  //Get the "frag" number
  while(isdigit((my_req->file)[end_pos])){
    end_pos++;
  }
  getSubstring(str_frag_num, my_req->file, start_pos, end_pos);

  my_req->segno = atoi(str_seq_num);
  my_req->fragno = atoi(str_frag_num);
}

/* @brief Parses the client's message and stores the info in the state.
 * @param client: Struct where all the info will be stored.
 */
void parse_client_message(struct state *client){
  char response[BUF_SHORT];
  char response2[BUF_SHORT];
  char *ext_loc;
  int  ext_pos;
  client_req *my_req = calloc(1, sizeof(client_req));

  //  printf("Received from client : %s \n", client->request);

  memset(client->response, 0, BUF_SIZE);
  memset(response, 0, BUF_SHORT);
  memset(response2, 0, BUF_SHORT);
  copy_info(my_req, client);
  char *fragment = strstr(client->uri, "Seg");
  char *manifest = strstr(client->uri, ".f4m");

  if(manifest){
    my_req->content_type = 1;
    my_req->segno = -1;
    my_req->fragno = -1;
  } else if(fragment){
    my_req->content_type = 2;
    parse_URI(my_req);
    parse_file(my_req);
  } else if(!manifest && !fragment){
    my_req->content_type = 0;
    my_req->segno = -1;
    my_req->fragno = -1;
  } else {
    //Impossible
  }

  if(manifest){
    ext_loc = strstr(my_req->file, ".");
    //ASSERT(ext_loc != NULL)
    ext_pos = ext_loc - (my_req->file);
    getSubstring(response2, my_req->file, 0, ext_pos);
    sprintf(response2, "%s%s_nolist.f4m", my_req->path, response2);
    sprintf(response, "GET %s HTTP/1.1\r\n", my_req->URI);
    sprintf(response, "%s%sGET %s HTTP/1.1\r\n%s", response, client->header, response2, client->header);
    client->servst->expecting = REGF4M;

  } else if(fragment){

    if(my_req->bitrate == 0)
      {
        my_req->bitrate = global_best;
        client->current_best = global_best;
      }

    sprintf(response, "GET %s%dSeg%d-Frag%d HTTP/1.1\r\n%s", my_req->path,
            my_req->bitrate, my_req->segno, my_req->fragno, client->header);
    client->servst->expecting = VIDEO;

    bzero(client->lastchunk, 200);
    snprintf(client->lastchunk, 200, "%s%dSeg%d-Frag%d",
             my_req->path,
             my_req->bitrate, my_req->segno, my_req->fragno);
  }
  else if(!manifest && !fragment){
    sprintf(response, "GET %s HTTP/1.1\r\n%s", my_req->URI, client->header);
    client->servst->expecting = VIDEO;

  } else {
    //Impossible
  }
  free(my_req);
  memcpy(client->response, response, strlen(response));
  client->resp_idx = strlen(response);
  //printf("Sending to server: %s \n", response);
}
