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

all: lisod

lisod: proxy.c logger.o engine.o parse.o
	$(CC) $(CFLAGS) proxy.c logger.o engine.o -o proxy

logger: logger.h logger.c
	$(CC) $(CFLAGS) logger.c -o logger.o

engine: engine.h engine.c
	$(CC) $(CFLAGS) engine.c -o engine.o

parse: parse.h parse.c
	$(CC) $(CFLAGS) parse.c -o parse.o

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
	rm -f *~ *.o *.tar lisod
