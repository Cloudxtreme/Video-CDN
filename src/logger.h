#define BUF_SIZE 8192

#include "proxy.h"

FILE* log_open(char* filename);

int log_close(FILE* file);

int log_state(fsm* state, FILE* file, double tput, char* chunkname);
