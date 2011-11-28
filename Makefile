OBJDIR = obj
OBJS = $(addprefix $(OBJDIR)/, dash.o parser.o pipes.o supportedcommands.o tokens.o)
DASH = dash

all: $(DASH)

$(DASH) : $(OBJS)
	gcc $(OBJS) -o $(DASH)

$(OBJDIR)/%.o: %.c
	gcc -c -g3 -Wall -Wextra $< -o $@

$(OBJS): | $(OBJDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:	
	rm -f $(OBJDIR)/*.o


