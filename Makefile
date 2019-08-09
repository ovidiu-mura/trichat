all: client server

client:
	gcc -g -Wall -pthread -o client client.c utils.c

server:
	gcc -g -Wall -pthread -o server server.c utils.c

clean:
	$(RM) client server *.o
