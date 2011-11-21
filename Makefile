all: 	clear compile run

clear:	
	rm -f *.o

compile:	
	gcc -g3 -o dash -Wall -Wextra dash.c supportedcommands.c tokens.c pipes.c

run:
	./dash 

