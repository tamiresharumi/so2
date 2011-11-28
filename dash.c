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

int current_num_jobs()
{
	int njobs = 0;
	struct job *j = jobs;
	while (j)
	{
		++njobs;
		j = j->next;
	}

	return njobs;
}


//pega o enésimo job da lista de jobs atual
struct job* get_job(int jobspec)
{
	int i = 0;
	struct job *j = jobs;

	while (i++ != jobspec)
		j = j->next;

	return j;
}

//pega o jobspec de um determinado job
int job_number(struct job *job)
{
	int i = 0;
	struct job *j = jobs;

	while (j != job)
	{
		j = j->next;
		++i;
	}
	
	return i;
}

void cd(char* param)
{
	chdir(param);
}

void dash_jobs()
{
	int i = 0;
	struct job *j = jobs;
	while (j)
	{
		printf("[%i] %i %s", i, j->processes->pid, j->command_line);
		j = j->next;
		++i;
	}

	foreground_job = 0;
}

//continua um job dado pelo command_jobspec, colocando ele em foreground se
//for o caso
void dash_continue_job(const char *command, const char *command_jobspec, int foreground)
{
	foreground_job = 0;

	if (!command_jobspec)
	{
		fprintf(stderr, "Usage: %s jobspec\n", command);
	}
	else
	{
		int jobspec = atoi(command_jobspec);
		if (jobspec >= current_num_jobs())
		{
			fprintf(stderr, "Unknown jobspec %i\n", jobspec);
		}
		else
		{
			struct job *j = get_job(jobspec);
			struct process *p;

			//manda um SIGCONT pra todos os processos desse job, pra eles
			//voltarem a rodar!
			for (p=j->processes ; p ; p=p->next)
			{
				p->stopped = FALSE;
				kill(p->pid, SIGCONT);
			}

			if (foreground)
			{
				//marca o job como job do foreground, pra poder ficar esperando
				//ele terminar daqui pra frente
				foreground_job = j;
			}
		}
	}
}

void dash_help(const char *help)
{
	if (help)
	{
		if (strequal(help, "cd"))
		{
			printf(
				"cd <target directory>\n\n\t"
				"Changes the current working diretory to <target directory>\n"
			);
		}
		else if (strequal(help, "bg"))
		{
			printf(
				"bg <jobspec>\n\n\t"
				"Puts the job given by <jobspec> to be run in the background\n"
			);
		}
		else if (strequal(help, "exit"))
		{
			printf(
				"exit\n\n\t"
				"Quits the current shell\n"
			);
		}
		else if (strequal(help, "bye"))
		{
			printf(
				"bye\n\n\t"
				"Quits the current shell with a cute message\n"
			);
		}
		else if (strequal(help, "fg"))
		{
			printf(
				"fg <jobspec>\n\n\t"
				"Puts the job given by <jobspec> to be run in the foreground\n"
			);
		}
		else if (strequal(help, "help"))
		{
			printf(
				"help [command]\n\n\t"
				"Shows help pages for internal commands\n"
			);
		}
		else if (strequal(help, "jobs"))
		{
			printf(
				"jobs\n\n\t"
				"Show the current jobs run by the shell and associated jobspecs\n"
			);
		}
		else
		{
			printf("Unknown dash internal command: '%s'\n", help);
		}
	}
	else
	{
		printf("DASH! DAAAAAAAAAAAAAAAAAAAAAASH ----- DASH!\n");
		printf("Help for DASH\n\n");
		printf("These are ---- DASH ----- internal commands\n\n");
		printf("bg <jobspec>\n");
		printf("cd <target directory>\n");
		printf("exit\n");
		printf("bye\n");
		printf("fg <jobspec>\n");
		printf("help\n");
		printf("jobs\n");
	}
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
		dash_jobs();
		return DASH_INTERNAL_OK;
	}
	else if (strcmp(command[0], "fg") == 0) {
		dash_continue_job(command[0], command[1], TRUE);
		return DASH_INTERNAL_OK;
	}
	else if (strcmp(command[0], "bg") == 0) {
		dash_continue_job(command[0], command[1], FALSE);
		return DASH_INTERNAL_OK;
	}
	else if (strcmp(command[0], "help") == 0) {
		dash_help(command[1]);
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
		//quando errno é EINTR, significa que a chamada waitpid retornou porque
		//o processo pelo qual se estava esperando foi interrompido, e isso é
		//um caso esperado, quando aperta CTRL-Z pra parar o processo que está
		//em foreground, então pode ignorar a mensagem de erro
		if (errno != EINTR)
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
			//e só avisa se o processo que terminou não for o processo que tá
			//em primeiro plano, porque quando tá em foreground dá pra ver que
			//terminou sem precisar de mensagem
			if (j != foreground_job)
				printf("Job [%i] done\n", j->processes->pid);
			else
				foreground_job = 0;
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

int is_stopped(struct job *job)
{
	int stopped = TRUE;
	struct process *p;

	for (p=job->processes ; p ; p=p->next) {
		if (!p->stopped && !p->finished) {
			stopped = FALSE;
			break;
		}
	}

	return stopped;
}

int is_finished(struct job *job)
{
	int finished = TRUE;
	struct process *p;

	for (p=job->processes ; p ; p=p->next) {
		if (!p->finished) {
			finished = FALSE;
			break;
		}
	}

	return finished;
}

//espera todos os processos desse job terminarem
void wait_job(struct job *job)
{
	int status;
	int pid;

	do {
		pid = waitpid(WAIT_ANY, &status, WUNTRACED);
	} while (
		!handle_job_status(pid, status)	&&
		!is_stopped(job) &&
		!is_finished(job)
	);
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

			fprintf(stderr, "Stopped [%i] %i %s\n",
				job_number(foreground_job),
				foreground_job->processes->pid,
				foreground_job->command_line
			);
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
					printf("Unexpected token '|'\n");
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
					//se tem um foreground job, é porque passou por um "fg",
					//então espera o processo terminar a partir de agora
					if (foreground_job)
						wait_job(foreground_job);
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

