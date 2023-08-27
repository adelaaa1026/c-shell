
CC = gcc
CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror
CFLAGS += -D_GNU_SOURCE 

PROMPT = -DPROMPT
EXECS = 33sh 33noprompt

.PHONY: all clean

all: $(EXECS)

33sh: sh.c jobs.c
	#TODO: compile your program, including the -DPROMPT macro
	$(CC) $(CFLAGS) $(PROMPT) $^ -o $@ 


33noprompt: sh.c jobs.c
	#TODO: compile your program without the prompt macro
	$(CC) $(CFLAGS) $^ -o $@ 


clean:
	#TODO: clean up any executable files that this Makefile has produced
	rm -f $(EXECS) 

