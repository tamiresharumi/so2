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
	DASH_INTERNAL_NOT_FOUND
};

enum dash_command execute_command(char** command, int *keep_running){
	
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
	else 
		return DASH_INTERNAL_NOT_FOUND;
}

int execute_process(const char *process, char * const * program_argv, int wait_for_finish)
{
	int pid = fork();
	if (pid == 0)
	{
		execvp(process, program_argv);
		perror("execvp");
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

void execute_piped_commands(char **commands, int num_commands, int num_pipes, int *starting_indices)
{
	int pipefd[2], oldfd[2];
	int i;
	int *pids = malloc((num_pipes+1) * sizeof(int));
	int status;

	//executa os processos 
	for (i=0 ; i<num_pipes+1 ; ++i)
	{
		const char *process = commands[starting_indices[i]];
		char **argvp;
		int j;
		//o número de argumentos é do índice atual até o próximo pipe ou
		//então até o fim da linha de comando, encontrar qual o certo
		int narg;
		if (i == num_pipes) {
			narg = num_commands - starting_indices[i];
		} else {
			narg = starting_indices[i+1] - starting_indices[i] - 1;
		}

		//o (+1) é pra garantir espaço para o '\0' que termina a lista
		argvp = malloc((narg+1) * sizeof(char*));

#if TEST_DASH_PIPE
		printf("[%s] : %i\n", process, narg);
#endif

		for (j=0 ; j<narg ; ++j)
		{
			char *current = commands[starting_indices[i]+j];
			int length = strlen(current) + 1;
			argvp[j] = malloc(length * sizeof(char));
			strncpy(argvp[j], current, length);
		}
		argvp[j] = 0;

#if TEST_DASH_PIPE
		for (j=0 ; j<=narg ; ++j)
			printf("\t[%i]: %s\n", j, argvp[j]);
#endif
		
		if (i < num_pipes)
		{
			pipe(pipefd);
		}

		pids[i] = fork();
		if (pids[i] == 0)
		{
			//se tem algum comando antes desse, refaz o stdin pro pipe
			if (i > 0)
			{
				dup2(oldfd[0], fileno(stdin));
				close(oldfd[0]);
				close(oldfd[1]);
			}
			//se tem algum comando depois desse, refaz o stdout pro pipe
			if (i < num_pipes)
			{
				close(pipefd[0]);
				dup2(pipefd[1], fileno(stdout));
				close(pipefd[1]);
			}

			execvp(process, argvp);
			perror("execvp");
			_exit(EXIT_FAILURE);
		}
		else
		{
			//se tem comando antes
			if (i > 0)
			{
				close(oldfd[0]);
				close(oldfd[1]);
			}
			//se tem comando depois
			if (i < num_pipes)
			{
				oldfd[0] = pipefd[0];
				oldfd[1] = pipefd[1];
			}
		}
	}

	//espera o primeiro processo terminar, fecha o pipe e depois espera
	//todos os processos que ele criou terminarem

	waitpid(pids[0], &status, 0);

	close(oldfd[0]);
	close(oldfd[1]);

	for (i=1 ; i<num_pipes+1 ; ++i)
		waitpid(pids[i], &status, 0);
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
				int num_pipes;
				enum dash_command internal_result;

				if (strequal(terminal[0], "|"))
				{
					printf("Caracter inseperado '|'\n");
					continue;
				}

				num_pipes = number_of_piped_commands(terminal, num_args, inst_limits);

				if (num_pipes > 0)
				{
					execute_piped_commands(terminal, num_args, num_pipes, inst_limits);
				}
				else
				{
					internal_result = execute_command(terminal, &keep_running);
					if (internal_result == DASH_INTERNAL_NOT_FOUND)
					{
						//não é comando interno do meu shell fofinho, executar do sistema
						//depois de checar que ele existe

						if (seek_command(terminal[0], path))
						{
							execute_process(terminal[0], terminal, TRUE);
						}
						else
						{
							printf("Comando %s nao encontrado!\n", terminal[0]);
						}
					}
				}
			}
		}
		else if(feof(stdin)) {
			printf("\n");			
			keep_running = 0;
		}
	}
	return EXIT_SUCCESS;
}

