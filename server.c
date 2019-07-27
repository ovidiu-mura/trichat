#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libs.h"


#define PORT 4444

struct sockaddr_in serverAddr;
struct sockaddr_in newAddr;
//char buffer[1024];
struct init_pkt *p;

int main(){
  unsigned char *d = malloc(1024);
  
  int sockfd, ret;
  int newSocket;

  socklen_t addr_size;
  char buffer[1024];
  pid_t childpid;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if(sockfd < 0){
    printf("[-]Error in connection\n");
    exit(1);
  }
  printf("[+]Server Socket is created.\n");

  memset(&serverAddr, '\0', sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = inet_addr("131.252.208.103");

  ret = bind(sockfd, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
  if(ret<0){
    printf("[-]Error in binding.\n");
    exit(1);
  }
  printf("[+]Bind to port %d\n", 4444);
  if(listen(sockfd, 10)==0){
    printf("[+]Listening...\n");
  }else{
    printf("[-]Error in binding.\n");
  }

  while(1){
    newSocket = accept(sockfd, (struct sockaddr*)&newAddr, &addr_size);
    if(newSocket < 0){
      exit(1);
    }
    printf("Connection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));

    if((childpid = fork())==0){
      close(sockfd);
      while(1){
        int n = recv(newSocket, buffer, 1024, 0);
	printf("bytes received %d\n", n);
	memcpy(d, buffer, n);
	d[n] = '\0';
	char *u = unhide_zeros((unsigned char*)d);
	for(int i=0; i<1024; i++)
	{
	  printf("%02x ", u[i]);
	}
//	p = deser_init_pkt(u);
        if(u[0] == 0x03)
	{
		struct data_pkt *pp = (struct data_pkt*)deser_data_pkt(u);
		printf("DATA PACKET:!!!!!!!!!\n");
		printf("type: %d\n", pp->type);
		printf("id: %d\n", pp->id);
		printf("src: %s\n", pp->src);
		printf("dst: %s\n", pp->dst);
		printf("data: %s\n", pp->data);
	}
	if(strcmp(buffer, ":exit") == 0){
	  printf("Disconnected %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));
	  break;
	} else {
	  printf("Client: %s\n", buffer);
	  send(newSocket, buffer, strlen(buffer), 0);
	  bzero(buffer, sizeof(buffer));
	}
      }
    }
  }
  close(newSocket);
  return 0;
}
