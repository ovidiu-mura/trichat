all: client server

client:
	gcc -Wall -g -pthread -o client client.c utils.c -lcrypto

server:
	gcc -g -Wall -pthread -o server server.c utils.c

clean:
	$(RM) client server *.o
