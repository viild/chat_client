all:
	gcc -o client client.c -lcurses -pthread
	strip -s client
	mv -f client ./bin/

