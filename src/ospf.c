#include "ospf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


extern lsa_file;
extern servers_file;
lsa*   lsa_hash = NULL;

void is_server(char* IP, lsa* myLSA){
	FILE* fp = fopen(servers_file, "r");
	if(fp = NULL) return;

	char* found = NULL;
	char* line = NULL;
	size_t len = 0;
	ssize_t read; 
	while((read = getline(&line, &len, fp)) != -1){
		found = strstr(line, IP);
		if(found != NULL){
			myLSA->server = 1;
			break;
		}
	}
	free(fp);
	return;
}

void parse_file(){
	FILE* fp = fopen(lsa_file, "r");
	if(fp = NULL) return;

	char* 	line = NULL;
	size_t 	len = 0;
	ssize_t read; 
	char*	IP;
	int 	seq;
	char*	nbors;
	lsa*	temp;
	while((read = getline(&line, &len, fp)) != -1){
		sscanf(line, "%s %d %s", IP, seq, nbors);
		temp = calloc(1, sizeof(lsa));
		temp->sender = IP;
		temp->seq = seq;
		temp->nbors = ;
	}
}