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
		_exit (EXIT_FAILURE);
	}
	if (validate_ip(argv[1]) && validate_input(argv[2]))
		connect_to_server(&connection,argv[1],argv[2]);
	else {
		perror("not a valid input");
		_exit(EXIT_FAILURE);
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
		_exit(EXIT_FAILURE);
	}

	printf("--!--\n");
	int nn = recv(connection.clientSocket, buffer, 1024, 0);
	unsigned char *un = unhide_zeros((unsigned char *)buffer);
	printf("received %d bytes, %02x\n", nn, un[0]);
	if(un[0] == 0x2)
	{
		printf("[+]Connected to Server.\n");
	} else {
		perror("[-]Failed to connect to Server\n");
		_exit(EXIT_FAILURE);
	}

	pthread_arg = (pthread_arg_t*) malloc(sizeof(pthread_arg_t));
	if(!pthread_arg)
	{
		perror("malloc for pthread_arg failed");
		_exit(EXIT_FAILURE);
	}
	pthread_arg->sockfd = connection.clientSocket;
	if(pthread_create(&recv_thread, 0, start_rtn, (void*)pthread_arg)){
		perror("creating new thread failed");
		free(pthread_arg);
		_exit(EXIT_FAILURE);
	}

	struct data_pkt pkt_3;
	int msg_start = 0;
	int isExit = 0;
	while(1){
		read(STDIN_FILENO, buffer, 1024);
		int i = 0;
		if(buffer[0] == '@'){
			while(buffer[i] != ' ')
				++i;
			buffer[i] = '\0';
			strcpy(pkt_3.dst, &buffer[1]);
			msg_start = i+1;
		} else
			strcpy(pkt_3.dst, ">>server**");
		while(buffer[i] != '\n')
			++i;
		buffer[i] = '\0';
		printf("%s %d\n", &buffer[msg_start], i-msg_start);
		if(strcmp(buffer, ":_exit") == 0) {
			isExit = 1;
		}
		if(!isExit) {
			pkt_3.type = DATA;
			pkt_3.id = getpid();
			strcpy(pkt_3.data, &buffer[msg_start]);
			strcpy(pkt_3.src, connection.username);
			char *data = ser_data(&pkt_3, DATA);
			char *serdat = hide_zeros(data);
			int no = send(connection.clientSocket, serdat, strlen(serdat), 0);
			printf("bytes sent %d\n", no);
			continue;
		}
		if(strcmp(buffer, ":_exit")==0){
			struct cls_pkt cls;
			cls.type = CLS;
			cls.id = 1;
			strcpy(cls.src, "client");
			strcpy(cls.dst, "server");
			char *serd = ser_data(&cls, CLS);
			char *hzcls = hide_zeros(serd);
			int n = send(connection.clientSocket, hzcls, strlen(hzcls), 0);
			if(n == strlen(hzcls)){
				close(connection.clientSocket);
				printf("[-]Disconnected from server.\n");
			} else {
				perror("[-]Failed to disconnect from server properly.\n");
				_exit(EXIT_FAILURE);
			}
			_exit(EXIT_FAILURE);
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
	signal(sig,SIG_IGN);
	char *str = "you pressed ctrl+c. Enter :exit to quit\n";
	write(STDOUT_FILENO,str,strlen(str));
}

void connect_to_server(connection_info * connection, char *serverAddr,char *port)
{
	get_userName(connection->username); 
	get_password(connection->password);
	if(!validate_username_password(connection->username,connection->password))
	{
		fprintf(stderr,"Invalid username or password\n");
		_exit(EXIT_FAILURE);
	}
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
}

void get_userName(char *username)
{
	char *str= "Enter a username: ";
	write(STDOUT_FILENO,str,strlen(str));
	fgets(username,20,stdin);
	if(strlen(username)>20){
		fprintf(stderr,"username must be 20 characters or less.\n");
		memset(username, 0, 20);
		get_userName(username);
	}
}
void get_password(char *password)
{
	char *str = "Enter your password: ";
	write(STDOUT_FILENO,str,strlen(str));
	fgets(password,20,stdin);
	
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
        unsigned char hashfilepass[SHA_DIGEST_LENGTH];
        unsigned char hashuserpass[SHA_DIGEST_LENGTH];
       	
	//calculate SHA1 of password
	SHA1(password,sizeof(password),hashuserpass);
       
        //Convert SHA1 byte into hex
        for (int i = 0; i < strlen(hashuserpass)-1; i++){
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
         while (!feof(fp)) /* read till end of file */
         {
            fscanf (fp,"%s %s",uname,hashfilepass);

	    if(strncmp(username,uname,strlen(uname)-1)==0)
	    {
	     if(strncmp(hashfilepass,output,strlen(hashfilepass)-1)==0)
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
}

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
