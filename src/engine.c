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

extern short listen_port;
extern short https_port;
extern char* cgipath;


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
  struct tm *Date; time_t t;
  struct tm *Modified;
  struct stat meta;
  char timestr[200] = {0}; char type[40] = {0};
  char* response = state->response;
  char* cgi = NULL; char* query = NULL;
  FILE *file;

  int pathlength = strlen(state->uri) + strlen(state->www) + strlen("/") + 1;
  char* path = malloc(pathlength);
  memset(path,0,pathlength);

  if(!strncmp(state->uri, "/", strlen("/")) && strlen(state->uri) == 1)
  {
    strncat(path,state->www,strlen(state->www));
    strncat(path,"/",strlen("/"));
    strncat(path,"index.html",strlen("index.html"));
  }
  else
  {
    /* Check for ? */
    cgi = memmem(state->uri, strlen(state->uri), "/cgi/", strlen("/cgi/"));
    query = memmem(state->uri, strlen(state->uri), "?", strlen("?"));

    if(cgi != NULL) // GET with cgi
    {
      if(query != NULL)
        strncat(path, state->uri, (query - state->uri));
      else
        strncat(path, state->uri, strlen(state->uri));
    }
    else
    { // Regular GET or HEAD
      if(!strncmp(state->method, "GET", strlen("GET")) ||
         !strncmp(state->method,"HEAD",strlen("HEAD")))
      {
        strncat(path, state->www, strlen(state->www));
        strncat(path,"/",strlen("/"));
        strncat(path,state->uri,strlen(state->uri));
      }
      else // POST
      {
        strncat(path, state->uri, strlen(state->uri));
      }
    }
  }

  t = time(NULL);
  Date = gmtime(&t);

  if (Date == NULL)
  {
    return 500;
  }

  /* Grab Date of message */
  if(strftime(timestr, 200, "%a, %d %b %Y %H:%M:%S %Z" ,Date) == 0)
  {
    return 500;
  }

  if(!strncmp(state->method,"GET",strlen("GET")) ||
     !strncmp(state->method,"HEAD",strlen("HEAD")))
  {

    if(!strncmp(state->method, "GET", strlen("GET")))
    {
      /* To CGI or not to CGI */
      if(cgi != NULL)
      {
        if(exec_cgi(state, path, 0))
          return 500;
      }
      else
      {
        /* Check if file exists */
        if(stat(path, &meta) == -1)
        {
          return 404;
        }

        /* Open uri specified by client and save it in state*/
        file = fopen(path,"r");
        state->body = malloc(meta.st_size); // free here brah
        state->body_size = meta.st_size;
        fread(state->body,1,state->body_size,file);
        fclose(file);
        addtofree(state->freebuf, state->body, FREE_SIZE);
      }
    }
    else // HEAD
    {
      state->body = NULL;
      state->body_size = 0;
    }

    if(cgi == NULL)
    {
      sprintf(response, "HTTP/1.1 200 OK\r\n");
      sprintf(response, "%sDate: %s\r\n", response, timestr);
      sprintf(response, "%sServer: Liso/1.0\r\n", response);

      if(!state->conn)
        sprintf(response, "%sConnection: close\r\n", response);
      else
        sprintf(response, "%sConnection: keep-alive\r\n", response);

      if(mimetype(state->uri, strlen(state->uri), type))
        sprintf(response, "%sContent-Type: %s\r\n", response, type);



      Modified = gmtime(&meta.st_mtime);
      sprintf(response, "%sContent-Length: %jd\r\n", response, meta.st_size);
      memset(timestr, 0, 200);
      if(strftime(timestr, 200, "%a, %d %b %Y %H:%M:%S %Z" , Modified) == 0)
      {
        return 500;
      }

      sprintf(response, "%sLast-Modified: %s\r\n\r\n", response, timestr);
    }

    state->resp_idx = (int)strlen(response);
  }
  else // We got a POST over here.
  {
    if(exec_cgi(state, path, 1))
      return 500;
    state->resp_idx = (int)strlen(response);
  }

  free(path);
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
int mimetype(char* file, size_t len, char* type)
{
  char* ext; size_t extlen; char* cgi;

  /* first check for cgi */
  cgi = memmem(file, len, "?", strlen("?"));

  if(cgi != NULL)
  {
    sprintf(type, "text/html"); return 1;
  }

  ext = memmem(file, len, ".", strlen("."));

  if(ext == NULL)
    return 0;

  extlen = len - (size_t)((ext + 1) - file);

  if((size_t)((ext+1) - file) == len)
  {
    return 0;
  }

  memcpy(type, ext+1, extlen);

  if(!strncmp(type,"html",strlen("html")))
  {
    sprintf(type, "text/html"); return 1;
  }

  if(!strncmp(type,"css",strlen("css")))
  {
    sprintf(type, "text/css"); return 1;
  }

  if(!strncmp(type,"png",strlen("png")))
  {
    sprintf(type, "image/png"); return 1;
  }

  if(!strncmp(type,"jpeg",strlen("jpeg")))
  {
    sprintf(type, "image/jpeg"); return 1;
  }

  if(!strncmp(type,"gif",strlen("gif")))
  {
    sprintf(type, "image/gif"); return 1;
  }

  return 0;
}

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

/*
  @brief Parses a CGI request given headers and path.

  @param        state     The data of the CGI request
  @param        filename  The CGI file to be executed
  @param        flag      0 -> GET; 1 -> POST

  @returns      -1 -> error; 0 -> success;
*/
int exec_cgi(fsm* state, char* filename, int flag)
{
  pid_t pid;
  int stdin_pipe[2];
  int stdout_pipe[2];

  char* ENVP[26] = {0}; // NULL terminate
  char* ARGV[ 2] = {cgipath, NULL};

  genenv(ENVP, state, filename, flag);

  /*************** BEGIN PIPE **************/
  /* 0 can be read from, 1 can be written to */
  if (pipe(stdin_pipe) < 0)
  {
    fprintf(stderr, "Error piping for stdin.\n");
    return -1;
   }

  if (pipe(stdout_pipe) < 0)
  {
    fprintf(stderr, "Error piping for stdout.\n");
    return -1;
   }
  /*************** END PIPE **************/

  /*************** BEGIN FORK **************/
  pid = fork();
  /* not good */
  if (pid < 0)
  {
    fprintf(stderr, "Something really bad happened when fork()ing.\n");
    return -1;
  }

  /* child, setup environment, execve  */
  if (pid == 0)
  {
     /*************** BEGIN EXECVE ****************/
    close(stdout_pipe[0]);
    close(stdin_pipe[1]);
    dup2(stdout_pipe[1], fileno(stdout));
    dup2(stdin_pipe[0], fileno(stdin));
    /* you should probably do something with stderr */

    /* pretty much no matter what, if it returns bad things happened... */
    if (execve(cgipath, ARGV, ENVP))
    {
      execve_error_handler();
      fprintf(stderr, "Error executing execve syscall.\n");
      exit(1);
    }
    /*************** END EXECVE ****************/
  }

   if (pid > 0)
   {
     fprintf(stdout, "Parent: Heading to select() loop.\n");
     close(stdout_pipe[1]);
     close(stdin_pipe[0]);

     if(!strncmp(state->method,"POST",strlen("POST")))
     {
       if (write(stdin_pipe[1], state->body, state->body_size) < 0)
       {
         fprintf(stderr, "Error writing to spawned CGI program.\n");
         return -1;
       }
     }

     fprintf(stderr,"Wrote successfully\n");
     fprintf(stderr,"Wrote: %s\n", state->body);

     close(stdin_pipe[1]); /* finished writing to spawn */
     int verbose = 0;
     if(verbose) {
     int readret;
     char buf[BUF_SIZE];
     while((readret = read(stdout_pipe[0], buf, BUF_SIZE-1)) > 0)
     {
       buf[readret] = '\0'; /* nul-terminate string */
       fprintf(stdout, "Got from CGI: %s\n", buf);
     }
     }
   }
   /*************** END FORK **************/

   /* Save the output file descriptor of the CGI process
      to read from later */
   state->pipefds = stdout_pipe[0];
   return 0;
 }

/***************************************************************/
/* @brief Generates environment variables and feeds it to ENVP */
/***************************************************************/
void genenv(char** ENVP, fsm* state, char* filename, int flag)
{
  /* There are 23 env vars to implement on minimum. Let's begin. */

  char* cgi = memmem(state->uri, strlen(state->uri), "?", strlen("?"));
  char* tmp; char* CRLF; size_t n;

  if(cgi != NULL && strlen(cgi) == 1) // Is the '?' at the end of the URI?
    cgi = NULL;

  if(flag)
  {// POST
    ENVP[0] = malloc(strlen("CONTENT_LENGTH=") + 20);
    memset(ENVP[0], 0, strlen("CONTENT_LENGTH=") + 20);
    snprintf(ENVP[0], strlen("CONTENT_LENGTH=") + 20, "CONTENT_LENGTH=%ld", state->body_size);
  }
  else     // GET
  {
    ENVP[0] = "CONTENT_LENGTH=";
  }

  if(flag)
  {
    tmp = search_hdr(state, "Content-Type: ", strlen("Content-Type: "));
    if(tmp != NULL)
    {
      CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

      if(CRLF == NULL)
        ENVP[1] = "CONTENT_TYPE=";

      n = (size_t)(CRLF - tmp);
      ENVP[1] = malloc(strlen("CONTENT_TYPE=") + n + 1);
      memset(ENVP[1], 0, strlen("CONTENT_TYPE=") + n + 1);
      addtofree(state->freebuf, ENVP[1], FREE_SIZE);

      snprintf(ENVP[1], strlen("CONTENT_TYPE=") + n + 1, "CONTENT_TYPE=%s", tmp);
    }
    else
      ENVP[1] = "CONTENT_TYPE=";
  }
  else
    ENVP[1] = "CONTENT_TYPE=";

  ENVP[2] = "GATEWAY_INTERFACE=CGI/1.1";

  /* QUERY-STRING */
  if(cgi == NULL) // POST
  {
    ENVP[3] = malloc(strlen("QUERY_STRING=") + state->body_size + 1);
    memset(ENVP[3], 0, strlen("QUERY_STRING=") + state->body_size + 1);
    addtofree(state->freebuf, ENVP[3], FREE_SIZE);
    sprintf(ENVP[3], "QUERY_STRING=%s", state->body);
  }
  else            // GET
  {
    ENVP[3] = malloc(strlen("QUERY_STRING=") + strlen(cgi+1) + 1);
    memset(ENVP[3], 0, strlen("QUERY_STRING=") + strlen(cgi+1) + 1);
    addtofree(state->freebuf, ENVP[3], FREE_SIZE);
    sprintf(ENVP[3], "QUERY_STRING=%s", cgi+1);
  }

  /* REMOTE_ADDR */
  ENVP[4] = malloc(strlen("REMOTE_ADDR=") + strlen(state->cli_ip) + 1);
  memset(ENVP[4], 0, strlen("REMOTE_ADDR=") + strlen(state->cli_ip) + 1);
  addtofree(state->freebuf, ENVP[4], FREE_SIZE);
  sprintf(ENVP[4], "REMOTE_ADDR=%s", state->cli_ip);

  /* REMOTE_HOST */
  ENVP[5] = "REMOTE_HOST=";

  /* REQUEST_METHOD */
  if(flag)
  {
    ENVP[6] = "REQUEST_METHOD=POST";
  }
  else
  {
    ENVP[6] = "REQUEST_METHOD=GET";
  }

  /* SCRIPT_NAME */
  ENVP[7] = "SCRIPT_NAME=/cgi";

  /* HOST_NAME */
  tmp = search_hdr(state, "Host: ", strlen("Host: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
      ENVP[8] = "HOST_NAME=";
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[8] = malloc(strlen("HOST_NAME=") + n + 1);
      memset(ENVP[8], 0, strlen("HOST_NAME=") + n + 1);
      addtofree(state->freebuf, ENVP[8], FREE_SIZE);

      snprintf(ENVP[8], strlen("HOST_NAME=") + n + 1, "HOST_NAME=%s", tmp);
    }
  }
  else
    ENVP[8] = "HOST_NAME=";

  /* SERVER_PORT */
  ENVP[9] = malloc(strlen("SERVER_PORT=") + 7); // Max digits in short
  memset(ENVP[9], 0, strlen("SERVER_PORT=") + 7);
  addtofree(state->freebuf, ENVP[9], FREE_SIZE);

  if(state->context == NULL) // http port
    sprintf(ENVP[9], "SERVER_PORT=%hd", listen_port);
  else                       // https port
    sprintf(ENVP[9], "SERVER_PORT=%hd", https_port);

  /* Server details */
  ENVP[10] = "SERVER_PROTOCOL=HTTP/1.1";
  ENVP[11] = "SERVER_NAME=Liso";

  /* HTTP_ACCEPT */
  tmp = search_hdr(state, "Accept: ", strlen("Accept: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[12] = "HTTP_ACCEPT=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[12] = malloc(strlen("HTTP_ACCEPT=") + n + 1);
      memset(ENVP[12], 0, strlen("HTTP_ACCEPT=") + n + 1);
      addtofree(state->freebuf, ENVP[12], FREE_SIZE);

      snprintf(ENVP[12], strlen("HTTP_ACCEPT=") + n + 1, "HTTP_ACCEPT=%s", tmp);
    }
  }
  else
    ENVP[12] = "HTTP_ACCEPT=";

  /* HTTP_REFERER */
  tmp = search_hdr(state, "Referer: ", strlen("Referer: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[13] = "HTTP_REFERER=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[13] = malloc(strlen("HTTP_REFERER=") + n + 1);
      memset(ENVP[13], 0, strlen("HTTP_REFERER=") + n + 1);
      addtofree(state->freebuf, ENVP[13], FREE_SIZE);

      snprintf(ENVP[13], strlen("HTTP_REFERER=") + n + 1, "HTTP_REFERER=%s", tmp);
    }
  }
  else
    ENVP[13] = "HTTP_REFERER=";

  /* HTTP_ACCEPT_ENCODING */
  tmp = search_hdr(state, "Accept-Encoding: ", strlen("Accept-Encoding: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[14] = "HTTP_ACCEPT_ENCODING=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[14] = malloc(strlen("HTTP_ACCEPT_ENCODING=") + n + 1);
      memset(ENVP[14], 0, strlen("HTTP_ACCEPT_ENCODING=") + n + 1);
      addtofree(state->freebuf, ENVP[14], FREE_SIZE);

      snprintf(ENVP[14], strlen("HTTP_ACCEPT_ENCODING=") + n + 1, "HTTP_ACCEPT_ENCODING=%s", tmp);
    }
  }
  else
    ENVP[14] = "HTTP_ACCEPT_ENCODING=";

  /* HTTP_ACCEPT_LANGUAGE */
  tmp = search_hdr(state, "Accept-Language: ", strlen("Accept-Language: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[15] = "HTTP_ACCEPT_LANGUAGE=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[15] = malloc(strlen("HTTP_ACCEPT_LANGUAGE=") + n + 1);
      memset(ENVP[15], 0, strlen("HTTP_ACCEPT_LANGUAGE=") + n + 1);
      addtofree(state->freebuf, ENVP[15], FREE_SIZE);

      snprintf(ENVP[15], strlen("HTTP_ACCEPT_LANGUAGE=") + n + 1, "HTTP_ACCEPT_LANGUAGE=%s", tmp);
    }
  }
  else
    ENVP[15] = "HTTP_ACCEPT_LANGUAGE=";

  /* HTTP_ACCEPT_CHARSET */
  tmp = search_hdr(state, "Accept-Charset: ", strlen("Accept-Charset: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[16] = "HTTP_ACCEPT_CHARSET=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[16] = malloc(strlen("HTTP_ACCEPT_CHARSET=") + n + 1);
      memset(ENVP[16], 0, strlen("HTTP_ACCEPT_CHARSET=") + n + 1);
      addtofree(state->freebuf, ENVP[16], FREE_SIZE);

      snprintf(ENVP[16], strlen("HTTP_ACCEPT_CHARSET=") + n + 1, "HTTP_ACCEPT_CHARSET=%s", tmp);
    }
  }
  else
    ENVP[16] = "HTTP_ACCEPT_CHARSET=";

  /* HTTP_COOKIE */
  tmp = search_hdr(state, "Cookie: ", strlen("Cookie: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[17] = "HTTP_COOKIE=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[17] = malloc(strlen("HTTP_COOKIE=") + n + 1);
      memset(ENVP[17], 0, strlen("HTTP_COOKIE=") + n + 1);
      addtofree(state->freebuf, ENVP[17], FREE_SIZE);

      snprintf(ENVP[17], strlen("HTTP_COOKIE=") + n + 1, "HTTP_COOKIE=%s", tmp);
    }
  }
  else
    ENVP[17] = "HTTP_COOKIE=";

  /* HTTP_USER_AGENT */
  tmp = search_hdr(state, "User-Agent: ", strlen("User-Agent: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[18] = "HTTP_USER_AGENT=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[18] = malloc(strlen("HTTP_USER_AGENT=") + n + 1);
      memset(ENVP[18], 0, strlen("HTTP_USER_AGENT=") + n + 1);
      addtofree(state->freebuf, ENVP[18], FREE_SIZE);

      snprintf(ENVP[18], strlen("HTTP_USER_AGENT=") + n + 1, "HTTP_USER_AGENT=%s", tmp);
    }
  }
  else
    ENVP[18] = "HTTP_USER_AGENT=";

  /* HTTP_CONNECTION */
  if(state->conn)
    ENVP[19] = "HTTP_CONNECTION=keep-alive";
  else
    ENVP[19] = "HTTP_CONNECTION=close";

  /* HTTP_HOST */
  tmp = search_hdr(state, "Host: ", strlen("Host: "));

  if(tmp != NULL)
  {
    CRLF = memmem(tmp, strlen(tmp), "\r\n", strlen("\r\n"));

    if(CRLF == NULL)
    {
      ENVP[20] = "HTTP_HOST=";
    }
    else
    {
      n = (size_t)(CRLF - tmp);
      ENVP[20] = malloc(strlen("HTTP_HOST=") + n + 1);
      memset(ENVP[20], 0, strlen("HTTP_HOST=") + n + 1);
      addtofree(state->freebuf, ENVP[20], FREE_SIZE);

      snprintf(ENVP[20], strlen("HTTP_HOST=") + n + 1, "HTTP_HOST=%s", tmp);
    }
  }
  else
    ENVP[20] = "HTTP_HOST=";

  /* What's left: */

  /* REQUEST_URI */
  ENVP[22] = malloc(strlen("REQUEST_URI=") + strlen(filename) + 1);
  memset(ENVP[22], 0, strlen("REQUEST_URI=") + strlen(filename) + 1);
  sprintf(ENVP[22], "REQUEST_URI=%s", filename);

  /* PATH_INFO (change) */
  ENVP[21] = malloc(strlen("PATH_INFO=") + strlen(filename) + 1);
  memset(ENVP[21], 0, strlen("PATH_INFO=") + strlen(filename) + 1);
  sprintf(ENVP[21], "PATH_INFO=%s", filename+4);

  ENVP[23] = "SERVER_SOFTWARE=Liso1.0";
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

void execve_error_handler()
{
    switch (errno)
    {
        case E2BIG:
            fprintf(stderr, "The total number of bytes in the environment \
(envp) and argument list (argv) is too large.\n");
            return;
        case EACCES:
            fprintf(stderr, "Execute permission is denied for the file or a \
script or ELF interpreter.\n");
            return;
        case EFAULT:
            fprintf(stderr, "filename points outside your accessible address \
space.\n");
            return;
        case EINVAL:
            fprintf(stderr, "An ELF executable had more than one PT_INTERP \
segment (i.e., tried to name more than one \
interpreter).\n");
            return;
        case EIO:
            fprintf(stderr, "An I/O error occurred.\n");
            return;
        case EISDIR:
            fprintf(stderr, "An ELF interpreter was a directory.\n");
            return;
        case ELIBBAD:
            fprintf(stderr, "An ELF interpreter was not in a recognised \
format.\n");
            return;
        case ELOOP:
            fprintf(stderr, "Too many symbolic links were encountered in \
resolving filename or the name of a script \
or ELF interpreter.\n");
            return;
        case EMFILE:
            fprintf(stderr, "The process has the maximum number of files \
open.\n");
            return;
        case ENAMETOOLONG:
            fprintf(stderr, "filename is too long.\n");
            return;
        case ENFILE:
            fprintf(stderr, "The system limit on the total number of open \
files has been reached.\n");
            return;
        case ENOENT:
            fprintf(stderr, "The file filename or a script or ELF interpreter \
does not exist, or a shared library needed for \
file or interpreter cannot be found.\n");
            return;
        case ENOEXEC:
            fprintf(stderr, "An executable is not in a recognised format, is \
for the wrong architecture, or has some other \
format error that means it cannot be \
executed.\n");
            return;
        case ENOMEM:
            fprintf(stderr, "Insufficient kernel memory was available.\n");
            return;
        case ENOTDIR:
            fprintf(stderr, "A component of the path prefix of filename or a \
script or ELF interpreter is not a directory.\n");
            return;
        case EPERM:
            fprintf(stderr, "The file system is mounted nosuid, the user is \
not the superuser, and the file has an SUID or \
SGID bit set.\n");
            return;
        case ETXTBSY:
            fprintf(stderr, "Executable was open for writing by one or more \
processes.\n");
            return;
        default:
            fprintf(stderr, "Unkown error occurred with execve().\n");
            return;
    }
}
