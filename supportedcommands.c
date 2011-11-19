#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char** chargeCommands(const char* file, int* size, int size_of_a_instruction)
{
	FILE* f;
	char **c;
	int i = 0;
	f = fopen(file, "r+");
	if(f == NULL)
	{
		printf("arquivo de definicao de comandos inexistente\n");
		return NULL;
	}
	c = malloc(size_of_a_instruction * sizeof(char*));
	while(!feof(f))
	{
		c[i] = malloc(10 * sizeof(char));
		fscanf(f, "%s", c[i]);
		i++;
	}
	c[i] = 0;
	*size = i - 1;
	return c;
}	

int founded(char* seek_cmd, char **list_cmd, int size_list)
{
	int i=0;
	for(i=0;i<size_list;i++)
	{
		if(strcmp(seek_cmd, list_cmd[i]) == 0)
			return 1;
	}
	return 0;
}
	
