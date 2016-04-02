
//Should calculate the bitrate by first finding the throughput and then
//comparing the result to the approprtiate bitrate in the global array.
int calculate_bitrate(struct timespec *start, struct timespec *end, int size){
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
