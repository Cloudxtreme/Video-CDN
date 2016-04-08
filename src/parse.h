/*********************************************************************/
/* @file parse.h                                                     */
/* @brief Contains interfaces for parse.c, which handles parsing GET */
/*        requests for bit-rate adaptation.                          */
/*********************************************************************/
#ifndef PARSE_H
#define PARSE_H

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "proxy.h"

#define BUF_SHORT 1024	//A small character buffer size

/* Stores information all the relevant information about the incoming request
 * from the client.
 * Fields:
 *		1. segno: The segment being requested, if any.
 *		2. fragno: The fragment being requested, if any.
 *		3. bitrate: Current client's bitrate.
 *		4. content_type: 2 if it's a chunk, 1 for manifest, 0 otherwise.
 *		5. req_type: The type of the request. For completeness. And soundness.
 *		6. URI: Path of the file/chunk/segment.
 *		7. version: HTTP version number.
 *		8. response: The response to send to the server.
 *		9. file: The file name.
 */
typedef struct client_req{
	int   segno;
	int   fragno;
	int   bitrate;
	int   content_type;
	char  req_type[BUF_SHORT];
	char  URI[BUF_SHORT];
	char  version[BUF_SHORT];
	char  *response;
	char  file[BUF_SHORT];
	char  path[BUF_SHORT];
} client_req;

void parse_f4m(fsm* state);
void calculate_bitrate(fsm* state);
void parse_client_message(struct state *client);

extern float alpha;



#endif
