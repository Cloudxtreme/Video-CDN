#define BUF_SIZE 8192

#include "proxy.h"

FILE* log_open(char* filename);

int log_close(FILE* file);

int log_state(fsm* state, FILE* file, unsigned long long tput, char* chunkname,
              unsigned int long long duration);

int log_dns(char* client_ip, char* response_ip, char* log_file);
