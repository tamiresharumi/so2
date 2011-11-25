#ifndef PARSER_H
#define PARSER_H

struct redirection
{
	const char *source;
	const char *destiny;
};

struct process
{
	char **argv;
	int pid;
	int finished;
	int stopped;
	int status;

	struct redirection *redirections;
	int num_redirections;
	
};

struct job
{
	struct process *processes;
	int num_processes;
};

struct job* build_job(char **commands, int num_commands);

#endif 

