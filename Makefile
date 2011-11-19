all: 	clear compile run

clear:	
	rm -f *.o

compile:	
	gcc -o dash -Wall -Wextra dash.c supportedcommands.c tokens.c pipes.c

run:
	./dash 

