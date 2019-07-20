#include "libs.h"

long PORT = 1084;

int sockfd;


struct sockaddr_in servaddr, cliaddr;


int main(int argc, char **argv)
{
  
  // Create socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0))<0){
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));
  memset(&cliaddr, 0, sizeof(cliaddr));

  // Filling server information
  servaddr.sin_family = AF_INET; // IPv4
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(PORT);

  // Bind the socket with the server address
  if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
  {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  char data[1024];
  int n, len;

  n = recvfrom(sockfd, NULL, 0, MSG_PEEK | MSG_TRUNC, (struct sockaddr*)&cliaddr, &len);
  printf("# of bytes received: %d\n",n);
  recvfrom(sockfd, &data, n, 0, (struct sockaddr*)(&cliaddr), &len);
  
  printf("server: received data, %d bytes; msg: %s\n", n, data);

  char *response = "response from server";
  char *send = malloc(100);
  strcpy(send, response);
  sendto(sockfd,send,1024,MSG_PEEK|MSG_TRUNC,(const struct sockaddr *)&cliaddr,sizeof(cliaddr));

  printf("server: response sent\n");

//  if(close(sockfd) != 0)
//    fprintf(stderr, "failed to close the server socket\n");

  return 0;
}


