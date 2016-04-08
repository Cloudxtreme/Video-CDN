#ifndef ENGINE_H
#define ENGINE_H

#include "proxy.h"

int   parse_line(fsm* state);
int   parse_headers(fsm* state);
int   parse_body(fsm* state);
int   store_request(char* buf, int size, fsm* state);
int   service(fsm* state);
void* memmem(const void *haystack, size_t hlen,
             const void *needle, size_t nlen);

int  resetbuf(fsm* state);
void clean_state(fsm* state);
int  mimetype(char* file, size_t len, char* type);
int  validsize(char* body_size);

int Recv(int fd, char* buf, int num);
int Send(int fd, char* buf, int num);

void addtofree   (char** freebuf, char* ptr, int bufsize);
void delfromfree (char** freebuf, int bufsize);

int   exec_cgi(fsm* state, char* filename, int flag);
void  genenv(char** ENVP, fsm* state, char* filename, int flag);
char* search_hdr(fsm* state, char* hdr, int n);

void execve_error_handler();

void store_request_serv(char* buf, int size, struct serv_rep* state);
int  parse_headers_serv(fsm* state);
int  parse_body_serv(struct serv_rep* servst, char* buf, ssize_t n);
int  resetbuf_serv(char* buf, int prefixlen, ssize_t totalen);

#endif
