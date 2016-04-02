#define BUF_SIZE 8192

FILE* log_open(char* filename);

int log_close(FILE* file);

int log_error(char* error, FILE* file);
