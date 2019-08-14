# CS510 - ALSP
# Team: Alex Davidoff, Kamakshi Nagar, Ovidiu Mura
# Date: 08/13/2019
#
# It builds the trichat app.

all: client server

client:
	gcc -Wall -g -pthread -o client client.c utils.c -lcrypto

server:
	gcc -g -Wall -pthread -o server server.c utils.c

clean:
	$(RM) client server *.o
