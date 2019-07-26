#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "libs.h"


#define PORT 4444

int main()
{
  int clientSocket, ret;
  struct sockaddr_in serverAddr;
  char buffer[1024];

  clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(clientSocket < 0){
	  printf("[-]Error in connection.\n");
	  exit(1);
  }
  printf("[+]Client Socket is created.\n");

  memset(&serverAddr, '\0', sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = inet_addr("131.252.208.103");

  ret = connect(clientSocket, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
  if(ret<0){
    printf("[-]Error in connection\n");
    exit(1);
  }
  printf("[+]Connect to Server.\n");
  struct init_pkt pkt_1;
  pkt_1.id = 1;
  pkt_1.type = INIT;
  printf("%02x\n", pkt_1.type);
  strcpy(pkt_1.src, "client1");
  strcpy(pkt_1.dst, "server");

  char *data = ser_data(&pkt_1, INIT);
  send(clientSocket, data, strlen(data), 0);
  while(1){
    printf("Client: \t");
    scanf("%s", &buffer[0]);
    send(clientSocket, buffer, strlen(buffer), 0);
    if(strcmp(buffer, ":exit")==0){
	    close(clientSocket);
      printf("[-]Disconnected from server.\n");
      exit(1);
    }

    if(recv(clientSocket, buffer, 1024, 0)<0){
      printf("[-]Error in receiving data.\n");
    }else{
      printf("Server: \t%s\n", buffer);
    }
  }
  return 0;
}

