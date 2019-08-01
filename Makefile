all: client server

client:
	gcc -o client client.c utils.c

server:
	gcc -pthread -o server server.c utils.c

clean:
	$(RM) client server *.o
