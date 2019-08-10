#include "libs.h"
#define PORT 4444

typedef struct connection_info
{
	int clientSocket;
	struct sockaddr_in serverAddr;
	char username[20];
}connection_info;

typedef struct pthread_arg_t
{
  int sockfd;
}pthread_arg_t;

char buffer[1024];
void INThandler(int);
void get_userName(char *username);
void set_userName(connection_info * connection);
void connect_to_server(connection_info * connection, char *serverAddr,char *port);
bool validate_input(char *a);

pthread_t recv_thread;
pthread_arg_t* pthread_arg;
pthread_mutex_t lock;
void *start_rtn(void*);

int main(int argc,char *argv[])
{
  pthread_mutex_init(&lock, 0);
	connection_info  connection;
	signal(SIGINT,INThandler);
	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s <IP> <port>\n", argv[0]);
		exit (EXIT_FAILURE);
	}
	if (validate_input(argv[2]))
		connect_to_server(&connection,argv[1],argv[2]);
	else {
		perror("not a valid input");
		exit(EXIT_FAILURE);
	}

	struct init_pkt pkt_1;
	pkt_1.id = 5;
	pkt_1.type = INIT;
	strcpy(pkt_1.src, connection.username);
	strcpy(pkt_1.dst, "server");
	char *data = ser_data(&pkt_1, INIT);
	char *d1 = hide_zeros(data);

	int n = send(connection.clientSocket, d1, strlen(d1), 0);
  if(n <= 0){
    perror("error sending INIT packet");
    exit(EXIT_FAILURE);
  }
  pthread_arg = (pthread_arg_t*) malloc(sizeof(pthread_arg_t));
  if(!pthread_arg)
  {
	  perror("malloc for pthread_arg failed");
    exit(EXIT_FAILURE);
	}
  pthread_arg->sockfd = connection.clientSocket;
  if(pthread_create(&recv_thread, 0, start_rtn, (void*)pthread_arg)){
		perror("creating new thread failed");
    free(pthread_arg);
    exit(EXIT_FAILURE);
  }

	struct data_pkt pkt_3;
  int msg_start = 0;
  char ttmp[20];

	while(1){
		read(STDIN_FILENO, buffer, 1024);
		int i = 0;
    if(buffer[0] == '@'){
      while(buffer[i] != ' ')
        ++i;
      strncpy(ttmp, buffer, i);
      ttmp[i] = '\0';
      strcpy(pkt_3.dst, &ttmp[1]);
      msg_start = i+1;
    }
    else{
      strcpy(pkt_3.dst, ">>server**");
      msg_start = 0;
    }
		while(buffer[i] != '\n')
      ++i;
		buffer[i] = '\0';
		printf("%s %d\n", &buffer[msg_start], i-msg_start);
		pkt_3.type = DATA;
		pkt_3.id = getpid();
		strcpy(pkt_3.data, &buffer[msg_start]);
		strcpy(pkt_3.src, connection.username);
		char *data = ser_data(&pkt_3, DATA);
		char *serdat = hide_zeros(data);
		int no = send(connection.clientSocket, serdat, strlen(serdat), 0);
		printf("bytes sent %d\n", no);
		if(strcmp(buffer, ":exit")==0){
			close(connection.clientSocket);
			printf("[-]Disconnected from server.\n");
			exit(1);
		}
	}
	return 0;
}

void *start_rtn(void *arg)
{
  int n;
  pthread_arg_t *pthread_arg = (pthread_arg_t*)arg;

  for(;;){
		if((n = recv(pthread_arg->sockfd, buffer, 1024, 0))<0){
			printf("[-]Error in receiving data.\n");
		} else if(buffer[0] = 0x03){
      pthread_mutex_lock(&lock);
      char *temp = unhide_zeros(buffer); 
      struct data_pkt *pkt = deser_data_pkt(temp);      
      printf("%s\n", pkt->data);
      pthread_mutex_unlock(&lock);
		}
    bzero(buffer, sizeof(buffer));
	}
}

void INThandler(int sig)
{
	char c[10];
	signal(sig,SIG_IGN);
	printf("you pressed ctrl+c. Enter :exit to quit\n");
}

void connect_to_server(connection_info * connection, char *serverAddr,char *port)
{
	get_userName(connection->username);
	connection->clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(connection->clientSocket < 0){
		printf("[-]Error in connection.\n");
		exit(1);
	}
	printf("[+]Client Socket is created.\n");

	connection->serverAddr.sin_family = AF_INET;
	connection->serverAddr.sin_port = htons(atoi(port));
	connection->serverAddr.sin_addr.s_addr = inet_addr(serverAddr);

	int ret = connect(connection->clientSocket, (struct sockaddr*)&connection->serverAddr,sizeof(connection->serverAddr));
	if(ret<0){
		printf("[-]Error in connection\n");
		exit(1);
	}
	printf("[+]Connected to Server.\n");
}

void get_userName(char *username)
{
  int i = 0;
	printf("Enter a username: ");
	fflush(stdout);
  fgets(username,20,stdin);
  while(username[i] != '\n')
    ++i;
  username[i] = '\0';
	if(strlen(username)>20){
		puts("username must be 20 characters or less.\n");
    memset(username, 0, 20);
    get_userName(username);
  }
}

bool validate_input(char *a)
{
	int i=0;
	if (a[i] == '-') i=1;//negative number	
	for(;a[i]!=0;i++)
	{
	    if(!isdigit(a[i]))
		    return false;
        } 
 return true;	
}
