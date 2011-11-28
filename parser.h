#ifndef PARSER_H
#define PARSER_H

enum redirection_type
{
	REDIRECTION_FD,  //file descriptor
	REDIRECTION_FILE //file
};

enum redirection_mode
{
	REDIRECTION_SIMPLE,
	REDIRECTION_APPEND,
	REDIRECTION_INPUT
};

struct redirection
{
	enum redirection_type src_type, dest_type;
	enum redirection_mode mode;
	const char *src;
	const char *dest;
	//o próximo da lista..
	struct redirection *next;
};

struct process
{
	struct process *next;

	char **argv;
	int pid;
	int finished;
	int status;

	struct redirection *redirections;
};

struct job
{
	struct process *processes;
	int background;
};

struct job* build_job(char **commands);

#endif 

