#include "parse.h"

//Should calculate the bitrate by first finding the throughput and then
//comparing the result to the approprtiate bitrate in the global array.
int calculate_bitrate(struct timespec *start, struct timespec *end, int size){
	return 0;
}

/*
	1. Break the message up into headers and body.
	2. Get the content length header.
	3. If not, just calculate that body length manually.
	4. Calculate the bitrate, set it and return.
*/
void parse_server_message(char *msg){
	return;
}

// Generate the client's message. Should be easy with the struct.
void gen_client_message(client_req *req){
	return;
}

/*
	1. Get the URI, request type, version, bitrate, blah.
	2. If possible, get the segno. Set the content_type value.
	3. Generate the server response.
*/
void parse_client_message() {
	return;
}

/*********************************************************/
/* @brief Parse a .f4m file to obtain bitrates.          */
/* @param state - state of the client to store bitrates. */
/* @param f4m   - The .f4m file.                         */
/*********************************************************/
void parse_f4m(fsm* state, FILE* f4m)
{
  char *buf = NULL, *needle1 = NULL, *needle2 = NULL;
  char[200] number = {0};
  size_t n  = 0, len = 0;
  int bitrate_f4m = 0;
  struct bitrate* rate = NULL;

  /* Getline allocates a null-terminated buffer for you internally. */
  while((getline(&buf, &n, f4m) != -1))
    {
      needle1 = strstr(buf, "bitrate=");

      if(!needle1)
        continue;

      needle2 = strstr(needle1 + strlen("bitrate=") + 1, "\"");
      len     = needle2 - (needle1 + strlen("bitrate=") + 1);

      bzero(number, 200);
      strncpy(number, needle1 + strlen("bitrate=") + 1, len);

      bitrate_f4m = atoi(number);

      rate          = calloc(sizeof(struct bitrate), 1);
      rate->bitrate = bitrate_f4m;
      HASH_ADD_INT(state->all_bitrates, bitrate, rate);
    }

  free(buf);
}
