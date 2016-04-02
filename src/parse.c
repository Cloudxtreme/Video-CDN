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
	int segno;
	int bitrate;
	char req_type[BUF_SHORT];
	char URI[BUF_SHORT];
	char version[BUF_SHORT];
	char content_type[BUF_SHORT];
	char *response;
} client_req;

//Should calculate the bitrate by first finding the throughput and then
//comparing the result to the approprtiate bitrate in the global array.
int calculate_bitrate(struct espec start, struct espec end, int size){
	return 0;
}

/*
	1. Break the message up into headers and body.
	2. Get the content length header.
	3. If not, just calculate that body length manually.
	4. Calculate the bitrate, set it and return.
*/
void parse_server_message(char *msg){
	return;
}

// Generate the client's message. Should be easy with the struct.
void gen_client_message(client_req *req){
	return;
}

/*
	1. Get the URI, request type, version, bitrate, blah.
	2. If possible, get the segno. Set the content_type value.
	3. Generate the server response.
*/
void parse_client_message(char *msg, int bitrate){
	return;
}