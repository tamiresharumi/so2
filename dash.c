#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include"supportedcommands.h"
#include "tokens.h"
#include "pipes.h"

#define MAX_PIPE 256
#define PROMPT " $ "
#define INST_SIZE 256

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


int execute_command(char** command){
	
	if(strcmp(command[0], "exit") == 0){
		return 0;
	}
	else if(strcmp(command[0], "bye") == 0){
		printf("Bye! \\o/\n");
		return 0;
	}
	else if(0 == strcmp(command[0], "cd")){
		cd(command[1]);
		return 1;
	}
	else return 1;
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
				if((num_inst = has_pipe(terminal, num_args, inst_limits))!= 0){
					for(i=0;i<num_inst;i++){
						printf("i:%d\n", i);
						fflush(stdin);
						printf("m ->%s\n", get_instruction(i, inst_limits, terminal));
					}
					//do something!
				}
				else //tirar esse else, acredito, quando eu arrumar o pipeT
				if(founded(terminal[0], commands, cadastred_comm)){
					keep_running = execute_command(terminal);
				}

				else if((find_in_path = seek_command(terminal[0], path)) != NULL){
					son = fork();
					if(son == 0){
						execvp(terminal[0], terminal);
					}
					else {
						int status;
						wait(&status);
					}
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
