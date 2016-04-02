/******************************************************************/
/* @file engine.c                                                 */
/*                                                                */
/* @brief Used to handle parsing and other heavy-lifting by LISO. */
/*                                                                */
/* @author Fadhil Abubaker                                        */
/******************************************************************/

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include "engine.h"

#define FREE_SIZE 40

/**********************************************************/
/* @brief Parses a given buf based on state and populates */
/* a struct with parsed tokens if successful. If request  */
/* is incomplete, stash it as its state.                  */
/*                                                        */
/* @param state The saved state of the client             */
/*                                                        */
/* @retval  0    if successful                            */
/* @retval -1    if incomplete request                    */
/* @retval 400   if malformed                             */
/* @retval 500   if internal error                        */
/* @retval 505   if wrong version                         */
/**********************************************************/
int parse_line(fsm* state)
{
  char* CRLF; char* tmpbuf;
  char* method; char* uri; char* version;
  size_t length;

  /* Check for CLRF */
  CRLF = memmem(state->request, state->end_idx, "\r\n\r\n", strlen("\r\n\r\n"));

  if(CRLF == NULL)
    return -1;

  /* We have a request, extract tokens */
  CRLF = memmem(state->request, state->end_idx,
                "\r\n", strlen("\r\n"));

  if(CRLF == NULL)
    return 500;

  /* Copy request line */
  length = (size_t)(CRLF - state->request);
  tmpbuf = strndup(state->request,length); // Remember to free here.

  /* Tokenize the line */
  method = strtok(tmpbuf," ");

  if (method == NULL)
  {free(tmpbuf); return 400;}

  /* Check if correct method */
  if (strncmp(method,"GET",strlen("GET")) && strncmp(method,"HEAD",strlen("HEAD"))
      && strncmp(method, "POST", strlen("POST")))
  {free(tmpbuf); return 501;}

  if((uri = strtok(NULL," ")) == NULL)
  {free(tmpbuf); return 400;}

  if((version = strtok(NULL, " ")) == NULL)
  {free(tmpbuf); return 400;}

  if(strncmp(version,"HTTP/1.1",strlen("HTTP/1.1")))
  {free(tmpbuf); return 505;}

  /* If there's one more token, malformed request */
  if(strtok(NULL," ") != NULL)
  {free(tmpbuf); return 400;}

  /* These are all malloced by strdup, so it is safe */
  state->method = method;
  state->uri = uri;
  state->version = version;
  addtofree(state->freebuf, tmpbuf, FREE_SIZE);

  return 0;
}


/*******************************************************************/
/* @brief   Parses headers from the request passed on by the state */
/* and populates the state accordingly                             */
/*                                                                 */
/* @retval  0   Success                                            */
/* @retval 411  No CL header (411)                                 */
/* @retval 400  malformed request (400)                            */
/* @retval 500  internal error (500)                               */
/*******************************************************************/
int parse_headers(fsm* state)
{
  char* CRLF; char* tmpbuf; char* body_size;
  char* hdr_start;

  size_t length;

  CRLF = memmem(state->request, state->end_idx, "\r\n\r\n", strlen("\r\n\r\n"));

  if(CRLF == NULL)
    return 500;

  /* First extract Connection: */

  tmpbuf = memmem(state->request, (int)(CRLF - state->request)+2, "Connection: close\r\n",
                  strlen("Connection: close\r\n"));

  if(tmpbuf == NULL)
    state->conn = 1; // Keep Alive
  else
    state->conn = 0; // Close

  /* Save the headers in state */

  hdr_start = memmem(state->request, state->end_idx, "\r\n", strlen("\r\n"));

  if(hdr_start == NULL)
    return 400;

  hdr_start += 2; // Go to the header line

  state->header = strndup(hdr_start, (size_t)(CRLF+4 - hdr_start));
  addtofree(state->freebuf, state->header, FREE_SIZE);

  if(strncmp(state->method,"POST",strlen("POST")))
  {
    return 0;
  }

  /* Now extract Content-Length: if POST */

  tmpbuf = memmem(state->request, state->end_idx, "Content-Length:",
                  strlen("Content-Length:"));

  if(tmpbuf == NULL)
    return 411;

  CRLF = memmem(tmpbuf, state->end_idx, "\r\n", strlen("\r\n"));
  length = (size_t)(CRLF - tmpbuf);
  tmpbuf = strndup(tmpbuf, length); // Free this guy please.

  if(strtok(tmpbuf," ") == NULL)
  {free(tmpbuf); return 411;}

  if((body_size = strtok(NULL, " ")) == NULL)
  {free(tmpbuf); return 411;}

  /* Check for valid Content-Length */
  if(!validsize(body_size))
  {free(tmpbuf); return 411;}

  /* If there's one more token, malformed request */
  if(strtok(NULL," ") != NULL)
  {free(tmpbuf); return 400;}

  state->body_size = (size_t)atoi(body_size);
  free(tmpbuf);

  return 0;
}


/**************************************************/
/* @brief parses the body and stores it in state. */
/* If the full body has not been read yet, stash  */
/* whatever has been received.                    */
/*                                                */
/* @retval  0    if successful                    */
/* @retval -1    if incomplete body               */
/* @retval 400   if malformed                     */
/**************************************************/
int parse_body(fsm* state)
{
  char* CRLF; char* body;

  /* CRLF should be here */
  CRLF = memmem(state->request, state->end_idx, "\r\n\r\n", strlen("\r\n\r\n"));

  if(CRLF == NULL)
    return 400;

  /* Check if there's a body to begin with */
  if(CRLF + 4 == state->request + state->end_idx)
    return -1;

  body = CRLF + 4;

  /* Check if the body is complete */
  if(body + (int) state->body_size > state->request + state->end_idx)
    return -1;

  state->body = strndup(body, state->body_size);
  addtofree(state->freebuf, state->body, FREE_SIZE);

  return 0;
}

/*
 */
int store_request(char* buf, int size, fsm* state)
{
  /* Check if request > BUF_SIZE */
  if(state->end_idx + size > BUF_SIZE)
    return -2;

  /* Store away in fsm */
  memcpy(state->request + state->end_idx, buf, size);
  state->end_idx += size;

  return 0;
}

/*********************************************************************/
/* @brief    Services requests obtained from the state of a client.  */
/* Services GET, HEAD and POST requests and populates the state with */
/* appropriate data.                                                 */
/*                                                                   */
/* @retval  0  success                                               */
/* @retval 500 internal server error                                 */
/* @retval 404 File not found                                        */
/*********************************************************************/
int service(fsm* state)
{

  if(!strncmp(state->method,"GET",strlen("GET")))
  {
    parse_client_message();
  }
  else // Non 'GET' request, just pass it on.
  {

  }

  return 0;
}

/*****************************************************************************/
/* @brief  populates the variable type with appropriate filetype information */
/*                                                                           */
/* @param file  The file to check for                                        */
/* @param len   length of 'type'                                             */
/* @param type  buffer to populate data                                      */
/*                                                                           */
/* @retval 0 if unrecognizable                                               */
/* @retval 1 if successful                                                   */
/*****************************************************************************/

int validsize(char* body_size)
{
  int size = atoi(body_size);

  if(size < 0)
    return 0;
  else
    return 1;
}


/*****************************************************************/
/* @brief   resetbuf resizes the buffer of a state using pointer */
/* arithmetic so that lisod can handle pipelined requests.       */
/*                                                               */
/* @returns length of newly resized buffer                       */
/*****************************************************************/
int resetbuf(fsm* state)
{
  char* CRLF; char* next; char* buf = state->request;
  int end = state->end_idx;
  size_t length;

  /* Since we've serviced at least one request, CRLF should show up */
  CRLF = memmem(buf, end, "\r\n\r\n", strlen("\r\n\r\n"));

  if (CRLF == NULL)
    return -1;

  if(strncmp(state->method, "POST", strlen("POST")))
  {
    /* length of the 1st request including CRLF */
    length = (size_t)(strlen("\r\n\r\n") + CRLF - buf);
  }
  else
  {
    /* length of the 1st request including CRLF + body */
    length = (size_t)(strlen("\r\n\r\n") + CRLF + state->body_size - buf);
  }

  if(length >= 8192)
  {
    /* buf was full, just memset */
    memset(buf, 0, BUF_SIZE);
    return 0;
  }

  if(strncmp(state->method, "POST", strlen("POST")))
      next = CRLF + 4;
  else
    next = CRLF + 4 + state->body_size;

  /* Copy remaining requests to start of buf */
  memcpy(buf,next,BUF_SIZE-length);

  /* Zero out the rest of the buf */
  memset(buf+(BUF_SIZE-length),0,length);

  /* I <3 this function */

  return strlen(buf);
}


/****************************************************/
/* @brief Cleans up state after serving one request */
/****************************************************/
void clean_state(fsm* state)
{
  memset(state->response, 0, BUF_SIZE);

  delfromfree(state->freebuf, FREE_SIZE);

  state->method = NULL;
  state->uri = NULL;
  state->version = NULL;
  state->header = NULL;

  state->body = NULL;
  state->body_size = 0;

  state->resp_idx = 0;
}

/******************************************************/
/* @brief searches headers for a particular field and */
/*        returns a ptr to its value.                 */
/*                                                    */
/* @param state       The state of the client         */
/* @param hdr         The string in question          */
/* @param n           The length of hdr               */
/******************************************************/
char* search_hdr(fsm* state, char* hdr, int n)
{
  char* CRLF;
  char* headers = state->header; // headers should be NULL terminated.
  char* needle  = memmem(headers, strlen(headers), hdr, n);

  if(needle == NULL)
    return NULL;

  needle += n;

  CRLF = memmem(needle, strlen(needle), "\r\n", strlen("\r\n"));

  if(CRLF == NULL)
    return NULL;

  return needle;
}

/*********************************************************/
/* @brief wrapper for reading HTTP / HTTPS sockets       */
/*                                                       */
/* @param fd             File descriptor for HTTP socket */
/* @param client_context SSL context                     */
/* @param buf            buffer for reading into         */
/* @param num            size of buffer                  */
/*********************************************************/
int Recv(int fd, SSL* client_context, char* buf, int num)
{
  if (client_context == NULL)
  {
    return recv(fd, buf, num, 0);
  }

  return SSL_read(client_context, buf, num);
}

/*********************************************************/
/*@brief wrapper for writing to HTTP / HTTPS sockets     */
/*                                                       */
/* @param fd             File descriptor for HTTP socket */
/* @param client_context SSL context                     */
/* @param buf            buffer for reading into         */
/* @param num            size of buffer                  */
/*********************************************************/
int Send(int fd, SSL* client_context, char* buf, int num)
{
  if(client_context == NULL)
  {
    return send(fd, buf, num, 0);
  }

  return SSL_write(client_context, buf, num);
}

void addtofree(char** freebuf, char* ptr, int bufsize)
{
  for (int i = 0; i < bufsize; i++)
  {
    if (freebuf[i] == NULL)
    {
      freebuf[i] = ptr;
      return;
    }
  }

  fprintf(stderr,"ADDTOFREE ERR");
  exit(0);
}

void delfromfree(char** freebuf, int bufsize)
{
  for (int i = 0; i < bufsize; i++)
  {
    if (freebuf[i] != NULL)
      free(freebuf[i]);
    freebuf[i] = NULL;
  }
  return;
}

/**********************************************************/
/* @returns NULL If not needle not found; else pointer to */
/* first occurrence of needle                             */
/* Code from stackoverflow.                               */
/**********************************************************/
void *memmem(const void *haystack, size_t hlen,
             const void *needle, size_t nlen)
{
    int needle_first;
    const void *p = haystack;
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = memchr(p, needle_first, plen - nlen + 1)))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p++;
        plen = hlen - (p - haystack);
    }

    return NULL;
}
