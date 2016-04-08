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
  if(fclose(file) != 0)
  {
    fprintf(stderr,"Error closing log file. \n");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int log_state(fsm* state, FILE* file, double tput, char* chunkname)
{

  struct timespec duration;
  duration.tv_sec  = state->end.tv_sec  - state->start.tv_sec;
  duration.tv_nsec = state->end.tv_nsec - state->start.tv_nsec;

  double avg_tput            = state->avg_tput;
  unsigned long long bitrate = state->current_best;
  char*  server              = state->serv_ip;

  fprintf(file, "%ld %lld.%.9ld %f %f %llu %s %s \n \n",
          time(NULL),
          (long long)duration.tv_sec,
          duration.tv_nsec,
          tput,
          avg_tput,
          bitrate,
          server,
          chunkname);

  fflush(file);

  return EXIT_SUCCESS;
}
