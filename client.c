#include "libs.h"
#define PORT 4444

typedef struct connection_info
{
	int clientSocket;
	struct sockaddr_in serverAddr;
	char username[20];
}connection_info;

char buffer[1024];
void INThandler(int);
void get_userName(char *username);
void set_userName(connection_info * connection);
void connect_to_server(connection_info * connection, char *serverAddr,char *port);
bool validate_input(char *a);

int main(int argc,char *argv[])
{
	connection_info  connection;
	signal(SIGINT,INThandler);
	if (argc != 3)
	{
		fprintf (stderr, "Usage: %s <IP> <port>\n", argv[0]);
		exit (EXIT_FAILURE);
	}
	if (strcmp(argv[1],"0.0.0.0")==0 && validate_input(argv[2]))
		connect_to_server(&connection,argv[1],argv[2]);
	else {
		perror("not a valid input");
		exit(EXIT_FAILURE);
	}

	struct init_pkt pkt_1;
	pkt_1.id = 5;
	pkt_1.type = INIT;
	strcpy(pkt_1.src, "client1");
	strcpy(pkt_1.dst, "server");
	char *data = ser_data(&pkt_1, INIT);
	char *d1 = hide_zeros(data);

	int n = send(connection.clientSocket, d1, strlen(d1), 0);

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
		//send(clientSocket, buffer, strlen(buffer), 0);
		int no = send(connection.clientSocket, serdat, strlen(serdat), 0);
		printf("bytes sent %d\n", no);
		if(strcmp(buffer, ":exit")==0){
			close(connection.clientSocket);
			printf("[-]Disconnected from server.\n");
			exit(1);
		}
		if(recv(connection.clientSocket, buffer, 1024, 0)<0){
			printf("[-]Error in receiving data.\n");
		} else {
			printf("Server: \t%s\n", buffer);
		}
	}
	return 0;
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
	printf("[+]Connect to Server.\n");
	set_userName(connection);
}
void get_userName(char *username)
{
	printf("Enter a username: ");
	fflush(stdout);
	//memset(username , 0, 1000);
	fgets(username,20,stdin);
	//trim_newline(username);
	if(strlen(username)>20)
		puts("username must be 20 characters or less.");
}
void set_userName(connection_info * connection)
{
	struct data_pkt pkt_3;
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
	//send(clientSocket, buffer, strlen(buffer), 0);
	int no = send(connection->clientSocket, serdat, strlen(serdat), 0);

	//strncpy(buffer,connection->username,strlen(connection->username));
	if(no<0){
		perror("send failure ");
		exit(1);
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


