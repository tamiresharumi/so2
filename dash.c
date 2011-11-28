#include "dash.h"
#include "parser.h"
#include "pipes.h"
#include "supportedcommands.h"
#include "tokens.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROMPT " $ "
#define INST_SIZE 256

//todos os jobs que estão abertos até o momento
struct job *jobs = 0;
struct job *foreground_job = 0;


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
	
	if (strcmp(command[0], "exit") == 0){
		*keep_running = FALSE;
		return DASH_INTERNAL_OK;
	}
	else if (strcmp(command[0], "bye") == 0){
		printf("Bye! \\o/\n");
		*keep_running = FALSE;
		return DASH_INTERNAL_OK;
	}
	else if (0 == strcmp(command[0], "cd")){
		cd(command[1]);
		return DASH_INTERNAL_OK;
	}
	else if (strcmp(command[0], "jobs") == 0) {
		int i = 0;
		struct job *j = jobs;
		while (j)
		{
			printf("[%i] %i %s", i, j->processes->pid, j->command_line);
			j = j->next;
			++i;
		}
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

void handle_redirects(struct redirection *redirect)
{
	for (/**/ ; redirect ; redirect = redirect->next)
	{
		int src_fd, dest_fd;

		//encontra o descritor de arquivo da origem
		if (redirect->src_type == REDIRECTION_FILE)
			src_fd = open(redirect->src, O_RDONLY);
		else
			src_fd = atoi(redirect->src);

		//encontra o descritor de arquivo do destino
		if (redirect->dest_type == REDIRECTION_FILE)
		{
			FILE *dest_file;

			if (redirect->mode == REDIRECTION_APPEND)
				dest_file = fopen(redirect->dest, "a");
			else if (redirect->mode == REDIRECTION_SIMPLE)
				dest_file = fopen(redirect->dest, "w");
			else
				dest_file = fopen(redirect->dest, "r");
			dest_fd = fileno(dest_file);
		}
		else
		{
			dest_fd = atoi(redirect->dest);
		}

		//duplica o descritor de arquivo pra fazer o redirecionamento
		if (dup2(dest_fd, src_fd) == -1)
		{
			perror("dup2");
		}
	}
}

void execute_job(struct job *job)
{
	struct process *p;
	struct process *first_process = job->processes;
	int pipefd[2], oldfd[2];

	for (p=job->processes ; p ; p=p->next)
	{
		//se tem outro processo mais pra frente
		//nesse job, cria o pipe
		if (p->next)
		{
			if (pipe(pipefd) == -1)
			{
				perror("pipe");
				return;
			}
		}

		pid_t pid = fork();
		if (pid == -1)
		{
			perror("fork");
		}
		else if (pid == 0)
		{
			//filho

			//se tem algum processo antes desse aqui, então tem que fazer o
			//stdin desse processo ler do pipe
			if (p != first_process)
			{
				//duplica o oldfd[0], que é a saída do pipe, pra ficar sobre o
				//stdin do processo que tá chamando essa função, ou seja, daqui
				//pra frente o stdin desse processo passa a ser uma cópia do
				//oldfd[0]
				dup2(oldfd[0], fileno(stdin));
				//como fez uma cópia do oldfd[0] aí em cima, precisa fechar uma
				//das cópias desse descritor de arquivo pra não ficar com
				//referências a mais pra esse arquivo. Tem que fechar todos os
				//descritores de arquivo que foram abertos e não vão ser
				//fechados automaticamente pelo processo, e isso inclui as
				//cópias. Como depois do fork esse filho aqui ficou com uma
				//cópia do descritor do pipe, precisa fechar a parte do pipe
				//que ele não usa também
				close(oldfd[0]);
				close(oldfd[1]);
			}

			//se tem algum processo depois desse, tem que fazer o stdout desse
			//processo aqui apontar pra entrada do próximo pipe
			if (p->next)
			{
				//fecha a ponta do pipe que não vai usar nesse processo
				close(pipefd[0]);
				//faz stdout desse processo ser uma cópia do pipefd[1] (entrada
				//do pipe)
				dup2(pipefd[1], fileno(stdout));
				//fecha uma cópia do descritor de arquivo pra não ficar com
				//referências a mais pra ele
				close(pipefd[1]);
			}

			//redirecionamentos tem que vir depois de redirecionar pro pipe!
			//http://wiki.bash-hackers.org/howto/redirection_tutorial#duplicating_file_descriptor_2_1
			handle_redirects(p->redirections);

			execvp(p->argv[0], p->argv);
			perror("execvp");
			exit(EXIT_FAILURE);
		}
		else
		{
			//pai
			p->pid = pid;
			
			//se tem algum processo antes do que foi aberto agora, fecha as
			//cópias que o pai tem dos descritores de arquivo pra não ficar com
			//referências extras, pra que o arquivo possa ser fechado
			//corretamente
			if (p != first_process)
			{
				close(oldfd[0]);
				close(oldfd[1]);
			}

			//se tem algum comando depois, guarda o novo pipe pra poder ser
			//acessado pelos próximos da sequência
			if (p->next)
			{
				oldfd[0] = pipefd[0];
				oldfd[1] = pipefd[1];
			}
		}
	}

	//fecha as pontas do pipe que ainda precisava se eles tiverem sido criados
	if (first_process->next)
	{
		close(oldfd[0]);
		close(oldfd[1]);
	}

	//terminou! aqui era só pra executar. esperar ou não é problema do shell ;)
}

void wait_job(struct job *job)
{
	struct process *p;
	int status;

	//WUNTRACED é pra fazer a waitpid retornar caso o processo tenha recebido
	//um SIGSTOP. Sem isso ela não retorna
	for (p=job->processes ; p ; p=p->next)
	{
		waitpid(p->pid, &status, WUNTRACED);
	}
}

int handle_job_status(int pid, int status)
{
	struct job *j;
	struct process *p;

	if (pid > 0) {
		for (j=jobs ; j ; j=j->next) {
			for (p=j->processes ; p ; p=p->next) {
				if (p->pid == pid)
				{
					p->status = status;
					if (WIFSTOPPED(status))
						p->stopped = TRUE;
					else
					{
						p->finished = TRUE;
					}

					return 0;
				}
			}
		}
	} else if (pid == 0 || errno == ECHILD) {
		//nenhum processo mudou de estado
		return -1;
	} else {
		perror("waitpid");
		return -1;
	}
	
	return -1;
}

void update_job_status()
{
	int status;
	int pid;
	do {
		pid = waitpid(WAIT_ANY, &status, WUNTRACED|WNOHANG);
	} while (!handle_job_status(pid, status));
}

void finish_jobs()
{
	struct job *j = jobs;
	struct job *last = 0, *next = 0;

	while (j)
	{
		int finished = TRUE;
		struct process *p = j->processes;

		next = j->next;

		//verifica se todos os processos desse job já terminaram
		while (p)
		{
			if (!p->finished)
			{
				finished = FALSE;
				break;
			}
			p = p->next;
		}

		if (finished)
		{
			//só mostra o pid do primeiro processo da coisa toda, mas deve
			//servir como referência.. (é que pode ser um caso com pipe, então
			//teria que ter mais que um pid..)
			printf("Job [%i] done\n", j->processes->pid);
			if (last)
				last->next = next;
			else
				jobs = next;
		}
		else
		{
			last = j;
		}

		j = next;
	}
}

void signal_handler(int sig, siginfo_t *si, void *unused)
{
	(void)si;
	(void)unused;
	
	if (sig == SIGTSTP)
	{
		if (foreground_job)
		{
			struct process *p = foreground_job->processes;
			while (p)
			{
				//manda os processos do job em primeiro plano pararem. Tadinhos
				kill(p->pid, SIGSTOP);
				p = p->next;
			}
		}
	}
}

int main(void)
{
	//trata o CTRL-Z com carinho
	struct sigaction sa;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sa.sa_sigaction = signal_handler;
	sigaction(SIGTSTP, &sa, 0);
	//ignora o CTRL-C
	signal(SIGINT, SIG_IGN);

	int num_args = 0;
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
				enum dash_command internal_result;
				struct job *job;

				//antes de fazer qualquer coisa, passa por todos os jobs e vê
				//se algum deles mudou de estado
				update_job_status();
				finish_jobs();

				if (strequal(terminal[0], "|"))
				{
					printf("Caracter inesperado '|'\n");
					continue;
				}
				
				job = build_job(terminal);
				job->command_line = strdup(getbuffer);
				
				if (job->processes && job->processes->next)
				{
					//tem mais que um processo na história, ou seja: pipes!
					execute_job(job);
				}
				else
				{
					internal_result = execute_command(terminal, &keep_running);
					if (internal_result == DASH_INTERNAL_NOT_FOUND)
					{
							execute_job(job);
					}
				}
				
				//adiciona o job na lista de jobs do shell
				if (internal_result == DASH_INTERNAL_NOT_FOUND)
				{
					if (jobs == 0)
					{
						jobs = job;
					}
					else
					{
						struct job *j = jobs;
						while (j->next)
							j = j->next;

						j->next = job;
					}
				}

				//decide se é pra esperar ou não
				if (internal_result == DASH_INTERNAL_OK)
				{
					//não espera porque já executou o que devia
				}
				else
				{
					//verifica se tem que deixar o job em background
					//ou foreground
					if (!job->background)
					{
						foreground_job = job;
						wait_job(job);
					}
					else
					{
						foreground_job = 0;
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

