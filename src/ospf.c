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

void parse_nbors(lsa* myLSA, char *nbors){
	int comma_count = 0;
	int token_count = 0;

	char* line 		= strstr(nbors, ",");
	char* token;
	while(line != NULL){
		comma_count++;
		line = strstr(line+1, ",");
	}

	myLSA->num_nbors = comma_count;
	myLSA->nbors = calloc(1, sizeof(char*));
	for(int i = 0; i <= comma_count; i++){
		myLSA->nbors[i] = calloc(1, MAX_IP_SIZE + 1);
	}

	token = strtok(nbors, ",");
	while(token){
		strcpy(myLSA->nbors[token_count], token);
		token = strtok(NULL, ",");
		token_count++;
	}
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
	lsa*	find;

	while((read = getline(&line, &len, fp)) != -1){
		sscanf(line, "%s %d %s", IP, seq, nbors);
		temp = calloc(1, sizeof(lsa));
		temp->sender = IP;
		temp->seq = seq;
		parse_nbors(temp, nbors);
		HASH_FIND_STR(lsa_hash, IP, find);
		if(find == NULL){
			HASH_ADD_STR(lsa_hash, sender, temp);
		} else {
			if(find->seq < temp->seq){
				for(int i = 0; i <= find->num_nbors; i++){
					free(find->nbors[i]);
				}
				free(find->nbors);
				find->nbors = temp->nbors;
				find->num_nbors = temp->num_nbors;
				find->seq = temp->seq;
			}
		}
	}
}