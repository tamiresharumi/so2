#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "pipes.h"
#include "dash.h"

int number_of_piped_commands(char **commands, int num_commands, int *starting_indices)
{
	int i;
	//Isso é pra ter um acesso uniforme, até
	//o primeiro comando tá incluído aqui
	int pipe_number = 1;
	starting_indices[0] = 0;

	for (i=0 ; i<num_commands ; ++i) {
		printf("commands[%i] : %s\n", i, commands[i]);
		if (strequal("|", commands[i])) {
			starting_indices[pipe_number] = i+1;
			++pipe_number;
		}
	}

	return pipe_number - 1;
}


int has_pipe(char** commands, int size, int vector_of_positions[])
{
	int i;
	int number_of_commands = 0;
	for(i=0 ; i<size ; i++)
	{
		if(strcmp(commands[i], "|") == 0) {
			vector_of_positions[number_of_commands] = (i - 1);
			number_of_commands++;
		}
	}

	vector_of_positions[number_of_commands] = (size - 1);
	return number_of_commands + 1;
}

char* get_instruction(int order_of_inst, int vector_of_positions[], char** commands)
{
	int i;
	char* command = malloc(256 * sizeof(char*));
	int begin_of_comm;
	int end_of_comm;
	//printf("Defining...\n");
	if(order_of_inst == 0)
		begin_of_comm = 0;
	else 
		begin_of_comm = vector_of_positions[order_of_inst - 1] + 2;
	end_of_comm = vector_of_positions[order_of_inst];

	//printf("Starting...\n");		
	for(i=begin_of_comm;i<=end_of_comm;i++){
		//printf("Command is %s in step %d\n", command, i);
		command = strcat(command, (const char*)commands[i]);
		command = strcat(command, " ");
	}
	return command;
}
