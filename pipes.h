#ifndef PIPES
#define PIPES

#define MAX_PIPE 256

int has_pipe(char** commands, int size, int vector_of_positions[]);
int number_of_piped_commands(char **commands, int num_commands, int *starting_indices);
char* get_instruction(int order_of_inst, int matrix_of_positions[], char** commands);

#endif
