all: client server

client:
	gcc -o client client.c utils.c

server:
	gcc -o server server.c utils.c

clean:
	$(RM) client server *.o
