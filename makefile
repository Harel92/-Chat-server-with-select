all: chatserver.c

	gcc chatserver.c -o chatserver

all-GDB: chatserver.c

	gcc -g chatserver.c -o chatserver