CFLAGS  = -Wall -g -U_FORTIFY_SOURCE
OBJ =  agpsup.o
PROG = agpsup

$(PROG): $(OBJ)
	$(CC) $(CFLAGS) -o $(PROG) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)
