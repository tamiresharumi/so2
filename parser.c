#include "parser.h"
#include "dash.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
//isso faz parte da libc, legal, né?
#include <regex.h>

//feio usar uma variável global.. Mas é pra recuperar do
//estrago que uma linha de comando mal feita pode fazer.
//até que vale a pena aqui pra não ter que refazer todo
//o parser
jmp_buf parse_environment;

enum parse_result
{
	PARSE_OK=0,
	PARSE_FAIL
};

/*
	Essa é a 'gramática' do shell

	inicio =
		(comando ( '|' comando)* background?)?
	comando =
		(palavra | redirection)+
*/

//cria e testa uma regexp contra um padrão
//	retorna
//		um regmatch* se deu certo, que tem que passar no free() depois
//		0 se deu algo errado
regmatch_t* regex_match(const char *input, const char *pattern)
{
	regex_t regex;
	regmatch_t *match = 0;
	int nargs;

	int error = regcomp(&regex, pattern, REG_EXTENDED);
	if (error != 0)
	{
		char buffer[0xff];
		regerror(error, &regex, buffer, 0xff);
		fprintf(stderr, "\033[1;31mError:\033[0m regcomp: %s\n", buffer);
		return 0;
	}

	nargs = regex.re_nsub + 1;
	match = malloc(nargs * sizeof(regmatch_t));

	error = regexec(&regex, input, nargs, match, 0);
	if (error != 0)
	{
		free(match);
		regfree(&regex);
		return 0;
	}

	return match;
}

char** get_next_token(char **next_token)
{
	
	return ++next_token;
}

char* copy_match(char **next_token, const regmatch_t match)
{
	char *copy = strndup(
		*next_token + match.rm_so,	// onde a string começa
		match.rm_eo - match.rm_so	// tamanho da string
	);
	return copy;
}

int empty_match(const regmatch_t match)
{
	if (match.rm_so == -1)
		return TRUE;
	else
		return FALSE;
}

int is_number(const char *input)
{
	regmatch_t *is = regex_match(input, "^[[:digit:]]+");
	if (is)
	{
		free(is);
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void parse_error()
{
	longjmp(parse_environment, PARSE_FAIL);
};

int parse_redirect(char ***next_token, struct process *process)
{
	struct redirection redirect;
	struct redirection **next = 0;

	regmatch_t *match = regex_match(**next_token,
		"^([[:digit:]]+)?(>&|>>|>|<)(.+)?"
	);
	if (match)
	{
		char *operator = copy_match(*next_token, match[2]);

		if (strequal(operator, ">"))
		{
			redirect.mode = REDIRECTION_SIMPLE;

			redirect.src_type = REDIRECTION_FD;
			if (empty_match(match[1]))
				redirect.src = strdup("1");
			else
				redirect.src = copy_match(*next_token, match[1]);

			redirect.dest_type = REDIRECTION_FILE;
			//levar em conta que o arquivo pra onde vai redirecionar pode estar
			//no token seguinte
			if (empty_match(match[3]))
			{
				*next_token = get_next_token(*next_token);
				if (**next_token == 0)
					parse_error();

				redirect.dest = strdup(**next_token);
			}
			else
			{
				redirect.dest = copy_match(*next_token, match[3]);
			}
		}
		else if (strequal(operator, ">>"))
		{
			redirect.mode = REDIRECTION_APPEND;
			
			redirect.src_type = REDIRECTION_FD;
			if (empty_match(match[1]))	
				redirect.src = strdup("1");
			else
				redirect.src = copy_match(*next_token, match[1]);

			redirect.dest_type = REDIRECTION_FILE;
			//levar em conta que o destino pode estar no próximo token
			if (empty_match(match[3]))
			{
				*next_token = get_next_token(*next_token);
				if (**next_token == 0)
					parse_error();

				redirect.dest = strdup(**next_token);
			}
			else
			{
				redirect.dest = copy_match(*next_token, match[3]);
			}
		}
		else if (strequal(operator, "<"))
		{
			redirect.mode = REDIRECTION_INPUT;

			redirect.src_type = REDIRECTION_FD;
			if (empty_match(match[1]))
				redirect.src = strdup("0");
			else
				redirect.src = copy_match(*next_token, match[1]);

			redirect.dest_type = REDIRECTION_FILE;
			//leva em conta que pode estar no próximo token
			if (empty_match(match[3]))
			{
				*next_token = get_next_token(*next_token);
				if (**next_token == 0)
					parse_error();

				redirect.dest = strdup(**next_token);
			}
			else
			{
				redirect.dest = copy_match(*next_token, match[3]);
			}
		}
		else if (strequal(operator, ">&"))
		{
			redirect.mode = REDIRECTION_SIMPLE;

			redirect.src_type = REDIRECTION_FD;
			if (empty_match(match[1]))
				redirect.src = strdup("1");
			else
				redirect.src = copy_match(*next_token, match[1]);

			//leva em conta que o destino pode estar no próximo token
			if (empty_match(match[3]))
			{
				*next_token = get_next_token(*next_token);
				if (**next_token == 0)
					parse_error();

				redirect.dest = strdup(**next_token);
			}
			else
			{
				redirect.dest = copy_match(*next_token, match[3]);
			}

			//verifica se é um número pra tratar como descritor de arquivo
			if (is_number(redirect.dest))
				redirect.dest_type = REDIRECTION_FD;
			else
				redirect.dest_type = REDIRECTION_FILE;
		}
	}
	else
	{
		free(match);
		return FALSE;
	}

	free(match);

	redirect.next = 0;

	//encontra onde que tem que adicionar o novo redirect
	next = &process->redirections;
	while (*next)
		next = &(*next)->next;
	
	assert(*next == 0);
	*next = malloc(sizeof(struct redirection));
	memcpy(*next, &redirect, sizeof(struct redirection));

	*next_token = get_next_token(*next_token);
	return TRUE;
}

//conta quantos argumentos o argv tem, sem contar o '\0' final
int count_args(char **argv)
{
	int number_args = 0;
	char **s;

	assert(argv != 0);

	for (s=argv ; *s!= 0 ; ++s)
		++number_args;

	return number_args;
}

int parse_word(char ***next_token, struct process *process)
{
	int num_args = count_args(process->argv);

	if (**next_token == 0         ||
		strstr(**next_token, "|") ||
		strstr(**next_token, "&"))
	{
		parse_error();
	}

	//adiciona +2 porque é um pro novo item e um pro '\0'
	process->argv = realloc(process->argv, (num_args + 2) * sizeof(char*));
	process->argv[num_args] = strdup(**next_token);
	process->argv[num_args+1] = 0;

	*next_token = get_next_token(*next_token);
	return TRUE;
}

int parse_command(char ***next_token, struct process *process)
{
	//verifica se o next_token é algum destes símbolos, porque se for é um
	//erro na linha de comando
	if (**next_token && strpbrk(**next_token, "|&><"))
	{
		fprintf(stderr, "Expected <command>, got %s\n", **next_token);
		parse_error();
	}

	while (**next_token && !strequal(**next_token, "|") && !strequal(**next_token, "&"))
	{
		if (!parse_redirect(next_token, process))
		{
			if (!parse_word(next_token, process))
			{
				parse_error();
			}
		}
	}

	//*next_token = get_next_token(*next_token);
	return TRUE;
}

struct process* new_process(struct job *job)
{
	struct process **next = &job->processes;
	struct process *newp;
	struct process *old = job->processes;

	while (*next)
		next = &(*next)->next;
	while (old && old->next)
		old = old->next;
	
	*next = malloc(sizeof(struct process));

	newp = *next;
	newp->next = 0;
	newp->argv = malloc(sizeof(char*));
	newp->argv[0] = 0;
	newp->finished = FALSE;
	newp->pid = 0;
	newp->redirections = 0;
	newp->status = 0;
	newp->stopped = 0;

	if (old)
		old->next = newp;

	return *next;
}

int parse_ampersand(char ***next_token, struct job *job)
{
	if (**next_token && strequal(**next_token, "&"))
	{
		job->background = TRUE;
		*next_token = get_next_token(*next_token);
	}

	return TRUE;
}

int parse_start(char **command, struct job *job)
{
	char ***next_token = &command;

	if (**next_token != 0)
	{
		struct process *process = new_process(job);
		if (!parse_command(next_token, process))
			return FALSE;

		while (**next_token && strequal(**next_token, "|"))
		{
			struct process *process = new_process(job);
			*next_token = get_next_token(*next_token);
			if (!parse_command(next_token, process))
				return FALSE;
		}

		parse_ampersand(next_token, job);
	}

	return TRUE;
}

void free_job(struct job *job)
{
	struct process *p;

	for (p=job->processes ; p ; p = p->next)
	{
		struct redirection *r;
		for (r=p->redirections ; r ; r = r->next)
		{
			
		}
	}
}

struct job* build_job(char **commands)
{
	int parse_result;

	struct job* new_job = malloc(sizeof(struct job));
	new_job->processes = 0;
	new_job->background = FALSE;
	new_job->next = 0;

	parse_result = setjmp(parse_environment);

	if (parse_result == PARSE_OK)
		parse_start(commands, new_job);
	else if (parse_result == PARSE_FAIL)
	{
		//ih, deu algo errado!
		free_job(new_job);
		free(new_job);
		return 0;
	}
	else
	{
		perror("setjmp");
	}

	return new_job;
}

