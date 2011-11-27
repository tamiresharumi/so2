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
	REDIRECTION_APPEND
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
	int stopped;
	int status;
	int background;

	struct redirection *redirections;
};

struct job
{
	struct process *processes;
};

struct job* build_job(char **commands);

#endif 

