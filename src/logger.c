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

int log_error(char* error, FILE* file)
{
  time_t now;
  time(&now);

  fprintf(file, "%s%s \n \n", ctime(&now), error);

  return EXIT_SUCCESS;
}
