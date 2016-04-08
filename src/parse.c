#include "parse.h"
#include "logger.h"

extern FILE* logfile;

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
      HASH_ADD_INT(state->all_bitrates, bitrate, rate);

      buf = needle2;
    }

}

/* Returns a substring of the given string from [start,end). */
void getSubstring(char *dest, char *src, int start, int end){
    strncpy(dest, src + start, end - start);
    strncat(dest, "\0", 1);
}

//Should calculate the bitrate by first finding the throughput and then
//comparing the result to the approprtiate bitrate in the global array.
void calculate_bitrate(fsm* state){

  int    size            = state->body_size;
  struct timespec *start = &(state->start);
  struct timespec *end   = &(state->end);
  unsigned long long int    bitrate;
  unsigned long long int    current_best = 0;

  unsigned long long int start_time =
    1000000000 * (start->tv_sec) + (start->tv_nsec);
  unsigned long long int end_time =
    1000000000 * (end->tv_sec) + (end->tv_nsec);
  unsigned int long long elapsed = end_time - start_time;

  double throughput = (size * 8 * 1000000) / elapsed; /* kilobits per second */

  struct bitrate* current = NULL;
  struct bitrate* tmp     = NULL;
  state->avg_tput = (alpha * throughput) + (1 - alpha)*(state->avg_tput);

  HASH_ITER(hh, state->all_bitrates, current, tmp) {
    /* This code loops through all struct bitrates */
    /* All bitrates are in units of Kbps */
    bitrate = current->bitrate;
    if((bitrate * 1.5) < state->avg_tput){
      {
        if(bitrate > current_best)
          current_best = bitrate;
      }
    }
  }

    state->current_best = current_best;

  log_state(state, logfile, throughput, state->lastchunk);
}

/* Copies some relevant information into my superior struct. */
void copy_info(client_req *my_req, struct state *client){
  memcpy(my_req->req_type, client->method, strlen(client->method));
  memcpy(my_req->URI, client->uri, strlen(client->uri));
  memcpy(my_req->version, client->version, strlen(client->version));
  memcpy(my_req->file, client->uri, strlen(client->uri));
  my_req->bitrate = client->current_best; //Change later
}

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

//Assumes we're dealing with video chunk
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

/*
	1. Get the URI, request type, version, bitrate, blah.
	2. If possible, get the segno. Set the content_type value.
	3. Generate the server response.
*/
void parse_client_message(struct state *client){
  char response[BUF_SHORT];
  char response2[BUF_SHORT];
  char *ext_loc;
  int  ext_pos;
  client_req *my_req = calloc(1, sizeof(client_req));

  printf("Received from client : %s \n", client->request);

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
  printf("Sending to server: %s \n", response);
}
