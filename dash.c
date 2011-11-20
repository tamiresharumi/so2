#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include"supportedcommands.h"
#include "tokens.h"
#include "pipes.h"
#include "dash.h"

#define PROMPT " $ "
#define INST_SIZE 256

int strequal(const char *str1, const char *str2)
{
	return 0 == strcmp(str1, str2);
}

char* seek_command(char* command, char* path){
	
	char* token = NULL;
	char* delim = ":";
	char *tmp = (char*)malloc(sizeof(char) * INST_SIZE);
	char *buf = (char*) malloc(sizeof(char) * strlen(path) + 1);
	strcpy(buf, path);
	token = strtok(buf, delim);
	while(token != NULL){
		strcpy(tmp, token);
		tmp = strcat(tmp, "/");
		tmp = strcat(tmp, command);
		if(access(tmp, F_OK) == 0){
			return tmp;
		}
		else token = strtok(NULL, delim);
	}
	return 0;
}

void cd(char* param)
{
	chdir(param);
}

enum dash_command
{
	DASH_INTERNAL_OK,
	DASH_NOT_FOUND
};

int execute_command(char** command, int *keep_running){
	
	if(strcmp(command[0], "exit") == 0){
		*keep_running = FALSE;
		return DASH_INTERNAL_OK;
	}
	else if(strcmp(command[0], "bye") == 0){
		printf("Bye! \\o/\n");
		*keep_running = FALSE;
		return DASH_INTERNAL_OK;
	}
	else if(0 == strcmp(command[0], "cd")){
		cd(command[1]);
		return DASH_INTERNAL_OK;
	}
	else return 1;
}

int execute_process(const char *process, char * const * program_argv, int wait_for_finish)
{
	int pid = fork();
	if (pid == 0)
	{
		execvp(process, program_argv);
		perror("fork error");
		_exit(EXIT_FAILURE);
	}
	else
	{
		if (wait_for_finish)
		{
			int status;
			waitpid(pid, &status, 0);
			return status;
		}

		return pid;
	}
}


int main(void)
{
	//variavel para loops de teste
	int i;

	char *path = getenv("PATH");
	
	//variaveis para comandos que o shell implementa
	int cadastred_comm = 0;
	char **commands = chargeCommands("commands.txt", &cadastred_comm, INST_SIZE);

	//variaveis de pipe
	int pipefd[2];
	int num_inst;
	int inst_limits[MAX_PIPE];

	//variaveis x 
	int num_args = 0;
	char* find_in_path;
	pid_t son;
	char **terminal;
	char directory[INST_SIZE];
	char getbuffer[INST_SIZE];
	int keep_running = 1;
	while(keep_running)
	{
		getcwd(directory, INST_SIZE);
		printf("%s%s ", directory, PROMPT);
		
		if(fgets(getbuffer, INST_SIZE, stdin) != NULL){
			
			if((terminal = tokenize(getbuffer, &num_args)) != 0){
				int num_commands;


				if (strequal(terminal[0], "|"))
				{
					printf("Caracter inseperado '|'\n");
					continue;
				}

				num_commands = number_of_piped_commands(terminal, num_args, inst_limits);

				
				if(founded(terminal[0], commands, cadastred_comm)){
					keep_running = execute_command(terminal);
				}

				else if((find_in_path = seek_command(terminal[0], path)) != NULL){
					execute_process(terminal[0], terminal, TRUE);
				}
				else printf("Comando %s nao encontrado!\n", terminal[0]);
			}
		}
		else if(feof(stdin)){
			printf("\n");			
			keep_running = 0;
		}
	}
	return EXIT_SUCCESS;
}
