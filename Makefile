#########################################################
# Makefile											    										#
# 													    												#
# Description: Makefile for compiling the lisod server. #
# 													    												#
# Author: Fadhil Abubaker.							    						#
#########################################################

CC			= gcc
CFLAGS 	= -Wall -Wextra -Werror -g -std=gnu99
VPATH 	=	src
OBJS		= proxy.o logger.o parse.o engine.o

all: proxy

# Implicit .o target
.c.o:
	$(CC) -c $(CFLAGS) $<

proxy: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

handin:
	(make clean; cd ..; tar cvf fabubake.tar 15-441-project-1 --exclude cp1_checker.py --exclude starter_code --exclude www --exclude flaskr --exclude handin.txt --exclude logfile --exclude ".gdbinit" --exclude ".gitignore" --exclude cgi_script.py --exclude cgi_example.c --exclude daemonize.c);

test1: proxy
	./proxy 9999 9998 logfile lockfile www ./flaskr/flaskr.py grader.key grader.crt

test2: proxy
	./proxy 9999 9998 logfile lockfile www cgi_script.py grader.key grader.crt

echo_client:
	$(CC) $(CFLAGS) echo_client.c -o echo_client

.PHONY: all clean

clean:
	rm -f *~ *.o *.tar proxy
