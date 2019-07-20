#include "libs.h"


struct sockaddr_in servaddr, cliaddr;
int sockfd;
long PORT = 1084;


int main(int argc, char **argv)
{
  //printf("argv 1 %s\n", argv[1]);
  // Creating socket file descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket creation failed");
    exit(EXIT_FAILURE);
  }

  memset(&servaddr, 0, sizeof(servaddr));

  // Filling server information
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(PORT);
  inet_pton(AF_INET, "131.252.217.212", &servaddr.sin_addr.s_addr);

  // send data to server
  char *data = "hellofromclient";
  char *d = malloc(100);
  strcpy(d, data);
  printf("%s\n", d);
  char server_response[1024];
  int n, len;

  sendto(sockfd,d,1024,MSG_PEEK|MSG_TRUNC,(const struct sockaddr*)&servaddr,sizeof(servaddr));
  printf("client: message sent.\n");

  n = recvfrom(sockfd, NULL, 0, MSG_PEEK | MSG_TRUNC, (struct sockaddr*)(&servaddr),&len);
  //Read packet
  recvfrom(sockfd, &server_response, n, 0, (struct sockaddr*)(&servaddr),&len);
  server_response[20] = '\0';
  printf("client: response received from server, %d bytes; msg: %s\n", n, server_response);
//  receive_data();
//  close_connection_client();
  if(close(sockfd) != 0)
    fprintf(stderr, "failed to close the client socket\n");

  return 0;
}
