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

int log_state(fsm* state, FILE* file, unsigned long long tput, char* chunkname,
              unsigned int long long duration)
{
  unsigned long long avg_tput = state->avg_tput;
  unsigned long long bitrate = state->current_best;
  char*  server              = state->serv_ip;

  double elapsed             = ((float) duration)/1000000000.0;

  if(strlen(chunkname) == 0)
    return 0;

  fprintf(file, "%ld %f %llu %llu %llu %s %s \n",
          time(NULL),
          elapsed,
          tput,
          avg_tput,
          bitrate,
          server,
          chunkname);

  fflush(file);

  return EXIT_SUCCESS;
}

int log_dns(char* client_ip, char* response_ip, char* log_file)
{
  FILE* fp = fopen(log_file, "a");

  fprintf(fp, "%ld %s video.cs.cmu.edu %s \n",
          time(NULL),
          client_ip,
          response_ip
          );
  fclose(fp);

  return 0;
}
