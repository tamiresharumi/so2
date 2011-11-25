#include "parser.h"
#include <string.h>
#include <stdlib.h>

struct job* build_job(char **commands, int num_commands)
{
	struct job* new_job;
	int size_command;
	int i, j;
	int old_instruction = 0;
	struct process new_proc;

	for(i=0;i<num_commands;i++)
	{
		//commands aqui recebe a linha digitada toda
		if(strcmp(commands[i], "|") == 0)
		{
			struct process new_proc;
			 new_proc.argv = (char**)malloc(sizeof(char*) * i);
			for(j=old_instruction;j<i;j++)
			{
				size_command = strlen(commands[j]);
				new_proc->argv[j] = (char*) 
					malloc(sizeof(char) * size_command + 1);
				strcpy(new_proc->argv[j], commands[j]);
			}
			old_instruction = i + 1;
		}
	}
	return new_job;
}

/*
struct process* find_process(pid_t pid)
{
	int i;
	process proc = job.processes;
	for(i=0;(i<job.num_processes)&&(proc.pid != pid);i++)
	{
		proc = proc.next;		
	}
}
*/


