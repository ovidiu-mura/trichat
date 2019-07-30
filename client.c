#include "libs.h"
#define PORT 4444

char buffer[1024];

int main()
{
  int clientSocket, ret;
  struct sockaddr_in serverAddr;

  clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(clientSocket < 0){
	  printf("[-]Error in connection.\n");
	  exit(1);
  }
  printf("[+]Client Socket is created.\n");

  memset(&serverAddr, '\0', sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(PORT);
  serverAddr.sin_addr.s_addr = inet_addr("0.0.0.0");

  ret = connect(clientSocket, (struct sockaddr*)&serverAddr,sizeof(serverAddr));
  if(ret<0){
    printf("[-]Error in connection\n");
    exit(1);
  }
  printf("[+]Connect to Server.\n");

  struct init_pkt pkt_1;
  pkt_1.id = 5;
  pkt_1.type = INIT;
  strcpy(pkt_1.src, "client1");
  strcpy(pkt_1.dst, "server");
  char *data = ser_data(&pkt_1, INIT);
  char *d1 = hide_zeros(data);
  
  int n = send(clientSocket, d1, strlen(d1), 0);

  struct data_pkt pkt_3;
  while(1){
    printf("Client: \t");
    read(STDIN_FILENO, buffer, 1024);
    int i = 0;
    while(buffer[i] != '\n')
    {
      i+=1;
    }
    buffer[i] = '\0';
    printf("%s %d\n", buffer, i);
    pkt_3.type = DATA;
    pkt_3.id = 35;
    memcpy(&pkt_3.data, &buffer, 300);
    strcpy(pkt_3.src, "client21");
    strcpy(pkt_3.dst, ">>server**");
    char *data = ser_data(&pkt_3, DATA);
    char *serdat = hide_zeros(data);
    int no = send(clientSocket, serdat, strlen(serdat), 0);
    printf("bytes sent %d\n", no);
    if(strcmp(buffer, ":exit")==0){
	    close(clientSocket);
      printf("[-]Disconnected from server.\n");
      exit(1);
    }

    if(recv(clientSocket, buffer, 1024, 0)<0){
      printf("[-]Error in receiving data.\n");
    } else {
      printf("Server: \t%s\n", buffer);
    }
  }
  return 0;
}

