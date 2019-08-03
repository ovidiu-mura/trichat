#include "libs.h"
#define MAXPENDING 10
#define MAXUSERS 20
#define PORT 4444

static const char *server_name = ">>server**";

typedef struct connection_info
{
	int sockfd;
	struct sockaddr_in serverAddr;
	char username[20];
}connection_info;

//char buffer[1024];
struct init_pkt *p;
void INThandler(int);
void startup(connection_info * connection,int port);
void* start_rtn(void* arg);

typedef struct pthread_arg_t 
{
	int sockfd;
	struct sockaddr_in newAddr;
	char *user; 
	int fd;
} pthread_arg_t;

typedef struct client_info {
  int fd;
  char *name;
}client_info;

int sockfd, newSocket, num_users;
struct sockaddr_in serverAddr, newAddr;
struct init_pkt *p;
pthread_attr_t pthread_attr;
pthread_arg_t* pthread_arg;
pthread_t pthread;
socklen_t len;
char **users;
pthread_mutex_t lock, msg_lock;
struct client_info *clients;
fd_set client_fds;
bool validate_input(char *a);

int new_user(char *name, int fd)
{
  if(!name)
    return 0;
  if(!users){
    users = malloc(MAXUSERS*sizeof(char*));
    clients = malloc(MAXUSERS*sizeof(struct client_info));
  }

  if(num_users == MAXUSERS){
    printf("Maximum users connected, cannot connect at this time\n");
    return -1;
  }
  
  for(int i = 0; i < num_users; ++i){
    if(!strcmp(users[i], name)){
      printf("%s already taken. Please enter another username: ", name);
      return 1; // To prompt again
    }
  }

  users[num_users] = malloc(strlen(name)+1);
  if(!users[num_users]){
    perror("malloc for new username failed");
    return -1;
  }
  strcpy(users[num_users], name);

  clients[num_users].name = malloc(strlen(name)+1);
  if(!clients[num_users].name){
    perror("malloc for new client name failed");
    return -1;
  }
  memcpy(clients[num_users].name, name, strlen(name));
//  clients[num_users].tid = pthread_self();
  clients[num_users].fd = fd;
  FD_SET(clients[num_users].fd, &client_fds);
  printf("New client: %s fd: %d\n", clients[num_users].name, clients[num_users].fd);
  ++num_users;
  return 0; 
}

int send_msg(struct data_pkt *pkt)//char *pkt->dst, char *msg, char *from)
{
  printf("msg to %s received\n", pkt->dst);
  int fd, n;

//  if(!pkt->dst || !msg)
//    return -1;
  
  for(int i = 0; i < num_users; ++i) {
    if(!strcmp(pkt->dst, clients[i].name)){/*
      strcat(pkt->dst, ".txt");
      if((fd = open(pkt->dst, O_RDWR|O_CREAT|O_TRUNC, S_IWUSR)) < 0){
        perror("Can't open user log file");
        break;
      }*/
      strcat(pkt->src, ": ");
      strcat(pkt->src, pkt->data);
      printf("sending %s to %s on fd %d\n", pkt->src, clients[i].name, clients[i].fd);
      
      n = send(clients[i].fd, pkt->src, strlen(pkt->src), 0);
      if(n > 0)
        return 0;
      printf("Something went wrong");
      break;
    } 
  }
  
  return -1;
}
    

int main(int argc, char *argv[])
{ 
	//int ret;
	int newSocket;
	connection_info connection;
	socklen_t addr_size;
	//char buffer[1024];
	//pid_t childpid;

	signal(SIGINT,INThandler);
	if (argc!=2)
	{ fprintf (stderr, "Usage: %s <port>\n", argv[0]);
		exit (EXIT_FAILURE);
	}
	if (validate_input(argv[1]))
		startup(&connection,atoi(argv[1]));
	else {
		perror("not a valid port number");
		exit(EXIT_FAILURE);
	}

	pthread_mutex_init(&lock, 0);
//  pthread_mutex_init(&msg_lock, 0);

	if(pthread_attr_init(&pthread_attr))
	{
		perror("pthread_attr_init failed");
		exit(EXIT_FAILURE);
	}

	if(pthread_attr_setdetachstate(&pthread_attr, PTHREAD_CREATE_DETACHED))
	{
		perror("pthread_attr_setdetachstate failed");
		exit(EXIT_FAILURE);
	}

	for(;;)
	{
		pthread_arg = (pthread_arg_t*) malloc(sizeof(pthread_arg_t));
		if(!pthread_arg)
		{
			perror("malloc for pthread_arg failed");
			continue;
		}

		len = sizeof(pthread_arg->newAddr);
		pthread_arg->sockfd = accept(connection.sockfd, (struct sockaddr*) &pthread_arg->newAddr, &len);

		if(pthread_arg->sockfd == -1)
		{
			perror("connecting to socket failed");
			free(pthread_arg);
			continue;
		}
		else
			printf("Connection accepted from %s:%d\n", inet_ntoa(newAddr.sin_addr), ntohs(newAddr.sin_port));

		if(pthread_create(&pthread, &pthread_attr, start_rtn, (void*)pthread_arg))
		{
			perror("creating new thread failed");
			free(pthread_arg);
			continue;
		}
	}

	//  close(sockfd); // Do in signal handler?

	return 0;
}
void INThandler(int sig)
{
	char c;
	signal(sig,SIG_IGN);
	printf("you pressed ctrl+c. Do you want to exit? [y/n]");
	c = getchar();
	if (c=='y' || c=='Y')
		exit(0);
	else
		signal(SIGINT,INThandler);
}
void startup(connection_info * connection,int port)
{
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connection->sockfd < 0)
	{
		printf("[-]Error in connection\n");
		exit(1);
	}
	printf("[+]Server Socket is created.\n");

	connection->serverAddr.sin_family = AF_INET;
	connection->serverAddr.sin_port = htons(port);
	connection->serverAddr.sin_addr.s_addr = INADDR_ANY;

	int ret = bind(connection->sockfd, (struct sockaddr*)&connection->serverAddr, sizeof(connection->serverAddr));
	if(ret<0)
	{
		printf("[-]Error in binding.\n");
		exit(1);
	}
	printf("[+]Bind to port %d\n", port);
	if(listen(connection->sockfd, 10)==0)
	{
		printf("[+]Listening...\n");
	}else
	{
		printf("[-]Error in binding.\n");
	}
}

void* thr_cleanup(char *d, pthread_arg_t *arg, int fd)
{
	if(d) free(d);
	if(arg) free(arg);
	close(fd);
	return 0;
}

void display_users()
{
  if(num_users == 0){
    printf("No other users currently online\n");
    return;
  }

  printf("Online Users: ");
  for(int i = 0; i < num_users; ++i){
    printf("\n%d) %s", i+1, clients[i].name);
  }
}

void* start_rtn(void* arg)
{
	unsigned char *d = malloc(1024), *u;
	pthread_arg_t* pthread_arg = (pthread_arg_t*) arg;
	int thr_sockfd, n;
	struct sockaddr_in thr_addr;
	char data[1024];

  pthread_mutex_lock(&lock);
  display_users();
  pthread_mutex_unlock(&lock);
  thr_sockfd = pthread_arg->sockfd;
	thr_addr = pthread_arg->newAddr;

	for(;;)
	{
		n = recv(thr_sockfd, data, 1024, 0);
    if(n == 0)
      break;
		printf("bytes received %d\n", n);
		memcpy(d, data, n);
		d[n] = '\0';
		u = unhide_zeros((unsigned char*)d);

/*		for(int i=0; i<1024; i++)
		{
			printf("%02x ", u[i]); 
		}
*/
    if(u[0] == 0x01){
      p = deser_init_pkt(u);
      pthread_mutex_lock(&lock);
      int ret = new_user(p->src, thr_sockfd);
      pthread_mutex_unlock(&lock);
      if(!ret)
        printf("User %s has connected\n", p->src); 
      else if(ret == 1)
        ;
      // PROMPT FOR ANOTHER NAME
      else
        return thr_cleanup(d, arg, thr_sockfd);
      printf("INIT PACKET:!!!!!!!!!\n");
      printf("type: %d\n", p->type);
      printf("id: %d\n", p->id);
      printf("src: %s\n", p->src);
      printf("dst: %s\n", p->dst);
    }

		if(u[0] == 0x03)
		{
			struct data_pkt *pp = (struct data_pkt*)deser_data_pkt(u);
    if(strcmp(pp->dst, server_name)){
      //  pthread_mutex_lock(&msg_lock);
        send_msg(pp);
     //   pthread_mutex_unlock(&msg_lock);
    }
			printf("DATA PACKET:!!!!!!!!!\n");
			printf("type: %d\n", pp->type);
			printf("id: %d\n", pp->id);
			printf("src: %s\n", pp->src);
			printf("dst: %s\n", pp->dst);
			printf("data: %s\n", pp->data);
			if(!strcmp(pp->data, ":exit"))
			{
				printf("Disconnected %s:%d\n", inet_ntoa(thr_addr.sin_addr), ntohs(thr_addr.sin_port));
				break;
			}
		} else
			if(!strcmp(data, ":exit"))
			{
				printf("Disconnected %s:%d\n", inet_ntoa(thr_addr.sin_addr), ntohs(thr_addr.sin_port));
				break;
			} else
			{
//				printf("Client: %s\n", data);
				send(thr_sockfd, data, strlen(data), 0);
				bzero(data, sizeof(data));
			}
	}

	return thr_cleanup(d, arg, thr_sockfd);
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
