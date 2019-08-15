/* CS510 - ALSP
 * Team: Alex Davidoff, Kamakshi Nagar, Ovidiu Mura
 * Date: 08/13/2019
 *
 * Trichat client connects to the server and execute commands, send
 * messages to the active online users.
 * Start the client command: ./client <server ip> <port>
 **/
#include "libs.h"

typedef struct connection_info
{
  int clientSocket;
  struct sockaddr_in serverAddr;
  char username[20];
  char password[20];
} connection_info;

typedef struct pthread_arg_t
{
  connection_info* conn;
  int sockfd;
} pthread_arg_t;

char buffer[1024];
void INThandler(int);
void get_userName(char *username);
void get_password(char *password);
bool validate_ip(char *a);
void connect_to_server(connection_info * connection, char *serverAddr, char *port);
bool validate_input(char *a);
bool validate_username_password(char *username,char *password);

pthread_t recv_thread;
pthread_arg_t* pthread_arg;
pthread_mutex_t lock;
void *start_rtn(void*);

int main(int argc, char *argv[])
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
    fprintf(stderr,"Invalid input");
    exit(EXIT_FAILURE);
  }
  struct init_pkt pkt_1;
  pkt_1.id = 5;
  pkt_1.type = INIT;
  strcpy(pkt_1.src, connection.username);
  strcpy(pkt_1.dst, "server");
  unsigned char *data = (unsigned char*)ser_data(&pkt_1, INIT);
  char *d1 = hide_zeros(data);
  int n = send(connection.clientSocket, d1, strlen(d1), 0);
  if(n <= 0){
    perror("error sending INIT packet");
    exit(EXIT_FAILURE);
  }

  int nn = recv(connection.clientSocket, buffer, 1024, 0);
  unsigned char *un = (unsigned char*)unhide_zeros((unsigned char *)buffer);
  printf("received %d bytes, %02x\n", nn, un[0]);
  if(un[0] == 0x2)
  {
    printf("[+]Connected to Server.\n");
  } else {
    printf("[-]Failed to connect to Server\n");
    exit(1);
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
  bool isExit = false;
  char ttmp[20];
  int space_count = 0;
  while(1){
    read(STDIN_FILENO, buffer, 1024);
    int i = 0;
    if(buffer[0] == '@'){
      while(buffer[i] != ' ')
      {
        ++i;
	space_count += 1;
	if(buffer[i] == '\n')
	  break;
      }
      if(space_count == 0)
        continue;
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
    if(strcmp(buffer, ":exit") == 0) {
      isExit = 1;
    }
    if(!isExit) {
      pkt_3.type = DATA;
      pkt_3.id = getpid();
      strcpy(pkt_3.data, &buffer[msg_start]);
      strcpy(pkt_3.src, connection.username);
      char *data = ser_data(&pkt_3, DATA);
      char *serdat = hide_zeros((unsigned char*)data);
      send(connection.clientSocket, serdat, strlen(serdat), 0);
      bzero(buffer, sizeof(buffer));
      continue;
    }
    if(strcmp(buffer, ":exit")==0){
      struct cls_pkt cls;
      cls.type = CLS;
      cls.id = 1;
      strcpy(cls.src, connection.username);
      strcpy(cls.dst, "server");
      char *serd = ser_data(&cls, CLS);
      char *hzcls = hide_zeros((unsigned char*)serd);
      int n = send(connection.clientSocket, hzcls, strlen(hzcls), 0);
      if(n == strlen(hzcls)){
        close(connection.clientSocket);
        printf("[-]Disconnected from server.\n");
	exit(1);
      } else {
        printf("[-]Failed to disconnect from server properly.\n");
	exit(-1);
      }
      exit(1);
    } else {
      bzero(buffer, sizeof(buffer));
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
    } else if(buffer[0] == 0x03){
    pthread_mutex_lock(&lock);
    char *temp = unhide_zeros((unsigned char*)buffer); 
    struct data_pkt *pkt = deser_data_pkt(temp);      
    printf("%s\n", pkt->data);
    pthread_mutex_unlock(&lock);
    } else if(buffer[0] == 0x02)
    {
      printf("ack packet data received.\n");
    }
    bzero(buffer, sizeof(buffer));
  }
}

void INThandler(int sig)
{
  signal(sig,SIG_IGN);
  printf("you pressed ctrl+c. Enter :exit to quit\n");
  bzero(buffer, sizeof(buffer));
}

void connect_to_server(connection_info * connection, char *serverAddr,char *port)
{
  get_userName(connection->username); 
  connection->clientSocket = socket(AF_INET, SOCK_STREAM, 0);
  if(connection->clientSocket < 0){
    perror("[-]Error in connection.\n");
    _exit(EXIT_FAILURE);
  }
  printf("[+]Client Socket is created.\n");

  connection->serverAddr.sin_family = AF_INET;
  connection->serverAddr.sin_port = htons(atoi(port));
  connection->serverAddr.sin_addr.s_addr = inet_addr(serverAddr);

  int ret = connect(connection->clientSocket, (struct sockaddr*)&connection->serverAddr,sizeof(connection->serverAddr));
  if(ret<0){
    perror("[-]Error in connection\n");
    _exit(EXIT_FAILURE);
  }
  printf("[+]Connected to Server.\n");
}

void get_userName(char *username)
{
  char *str= "Enter a username: ";
  int i = 0;

  write(STDOUT_FILENO,str,strlen(str));
  read(STDIN_FILENO,username,20);
  while(username[i] != '\n')
    ++i;
  username[i] = '\0';

  if(strlen(username)>20){
    fprintf(stderr,"username must be 20 characters or less.\n");
    memset(username, 0, 20);
    get_userName(username);
  }
}

/*void get_password(char *password)
  {
  char *str = "Enter your password: ";
  write(STDOUT_FILENO,str,strlen(str));
  read(STDIN_FILENO,password,20);

  if(strlen(password)>20){
  fprintf(stderr,"password must be 20 characters or less.\n");
  memset(password, 0, 20);
  get_password(password);
  }
  }

  bool validate_username_password(char *username,char *password)
  {
  char uname[20];
  char output[60];
  char *ptr = &output[0];

//calculate SHA1 of password
SHA1((unsigned char*)password,sizeof(password),hashuserpass);

//Convert SHA1 byte into hex
for (int i = 0; i < strlen((const char *)hashuserpass)-1; i++){
ptr += sprintf (ptr, "%02x", hashuserpass[i]);

} 
// convert to loser case
for(int i=0;i<40;i++)
{
output[i]=tolower(output[i]);
}

// open password file and comapre username and password
FILE *fp = fopen("password.txt","r");
if ( fp != NULL )
{
while (!feof(fp))  read till end of file 
{
fscanf (fp,"%s %s",uname,hashfilepass);

if(strncmp(username,uname,strlen(uname)-1)==0)
{
if(strncmp((const char *)hashfilepass,output,strlen((const char *)hashfilepass)-1)==0)
{
printf("Access Granted\n");
return true;
}
}
}  
fclose ( fp);
}
else
{
perror ( "file error" ); 
}
return false;
}*/

/*function to check numeric input*/
bool validate_input(char *a)
{
  int i=0;
  if (a[i] == '-') i=1;//negative number	
  for(;a[i]!='\0';i++)
  {
    if(!isdigit(a[i]))
      return false;
  }
  return true;
}

/* function to check the valid command-line IP*/
bool validate_ip(char *a)
{
  int num, dot = 0;
  char *ptr;

  //checks if input is null or had more than one dot
  if (a == NULL || strstr(a,"..")!=NULL)
    return false;

  ptr = strtok(a, ".");

  if (ptr == NULL)
    return false;

  while (ptr) {
    /* after parsing string, it must contain only digits */
    if (!validate_input(ptr))
      return false;

    if(strlen(ptr)>3)
      return false;

    num = atoi(ptr);

    /* check for valid IP */
    if (num >= 0 && num <= 255) {
      /* parse remaining string */
      ptr = strtok(NULL, ".");
      if (ptr != NULL)
        dot++;
    } else
      return false;
  }
  /* valid IP string must contain 3 dots */
  if (dot !=3)
    return false;
  return true;
}
