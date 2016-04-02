/*********************************************************************/
/* @file parse.h                                                     */
/* @brief Contains interfaces for parse.c, which handles parsing GET */
/*        requests for bit-rate adaptation.                          */
/*********************************************************************/
#ifndef PARSE_H
#define PARSE_H

#define BUF_SHORT 		1024	//A small character buffer size

/* Stores information all the relevant information about the incoming request
 * from the client.
 * Fields:
 *		1. segno: The segment being requested, if any.
 *		2. bitrate: Current client's bitrate.
 *		3. req_type: The type of the request. For completeness. And soundness.
 *		4. URI: Path of the file/chunk/segment.
 *		5. version: HTTP version number.
 *		6. content_type: 1 if it's a video fragment, 0 if it's anything else.
 *		7. response: The response to send to the server.
 */
typedef struct client_req{
	int   segno;
	int   bitrate;
	char  req_type[BUF_SHORT];
	char  URI[BUF_SHORT];
	char  version[BUF_SHORT];
	char  content_type[BUF_SHORT];
	char  *response;
} client_req;

typedef struct serv_rep {
  // Malek, plz make this.
} serv_rep;

int calculate_bitrate(struct timespec *start, struct timespec *end, int size);
void parse_server_message(char *msg);
void parse_client_message(char *msg, int bitrate);

#endif
