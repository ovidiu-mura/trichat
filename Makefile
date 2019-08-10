all: client server

client:
	gcc -Wall -pthread -o client client.c utils.c -lcrypto

server:
	gcc -Wall -pthread -o server server.c utils.c

clean:
	$(RM) client server *.o
