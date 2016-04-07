/*******************************************************************/
/*                                                                 */
/* @file logger.c                                                  */
/*                                                                 */
/* @brief Logger module to be used with liso. Outputs logging data */
/* to a specified file while handling errors.                      */
/*                                                                 */
/* @author Fadhil Abubaker                                         */
/*                                                                 */
/*******************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "logger.h"

FILE* log_open(char* filename)
{
  FILE* file = fopen(filename,"w+");

  if(file == NULL)
  {
    fprintf(stderr,"Error opening/creating log file. \n");
    exit(1);
  }

  return file;
}

int log_close(FILE* file)
{
  fprintf(file, "Closing log...\n");

  if(fclose(file) != 0)
  {
    fprintf(stderr,"Error closing log file. \n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int log_state(fsm* state, FILE* file, double tput, int bitrate, char* chunkname)
{

  struct timespec duration;
  duration.tv_sec  = state->end.tv_sec  - state->start.tv_sec;
  duration.tv_usec = state->end.tv_usec - state->start.tv_usec;

  float avg_tput = state->avg_tput;
  float bitrate  = state->bitrate;
  char* server   = state->serv_ip;

  fprintf(file, "[%u:] <duration:%lld.%.6ld> <tput:> \n \n",
          time(NULL),
          (long long)duration.tv_sec,
          duration.tv_usec,
          );

  return EXIT_SUCCESS;
}
