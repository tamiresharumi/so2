#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tokens.h"

void see_tokens(char *msg){
	
	int i=0;
	//printf("Im in tokenize");	
	char * delim = " ";
	char *result = NULL;
	char *buf = (char*) malloc(sizeof(char) * strlen(msg) + 1);
	strcpy(buf, msg);
	result = strtok(buf, delim);
	while(result != NULL){
		printf("%d:%s\n", i, result);
		i++;
		result = strtok(NULL, delim);
	}
}

char** tokenize(char *msg, int* return_number_of_parameters){
	
	int i=0;
	char ** returned;		
	char * delim = " \n";
	char *result = NULL;
	char *buffer = (char*) malloc(sizeof(char) * strlen(msg) + 1);
	strcpy(buffer, msg);

	result = strtok(buffer, delim);

	while(result != NULL){
		i++;
		result = strtok(NULL, delim);
	}
	*return_number_of_parameters = i;
	
	returned = (char**)malloc(sizeof(char*) * i);
	i = 0;
	strcpy(buffer, msg);
	result = strtok(buffer, delim);

	while(result != NULL){
		returned[i] = (char*)malloc(sizeof(char) * strlen(result) + 1);
		strcpy(returned[i], result);
		i++;
		result = strtok(NULL, delim);
	}
	if (i == 0)
		return 0;
	else return returned;
}
