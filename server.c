#include "libs.h"
#define MAXPENDING 10
#define MAXUSERS 20
#define TIMEOUT 1000

static const char *server_name = ">>server**";
static const char *msg_to_all = ">>broadcast**";

// For initial server connection
typedef struct connection_info
{
  int sockfd;
  struct sockaddr_in serverAddr;
  char username[20];
}connection_info;

struct init_pkt *p;
void INThandler(int);
void startup(connection_info * connection,int port);
void* start_rtn(void *arg);
void* accept_conn(void *arg);
void* do_reads(void *arg);
void* do_writes(void *arg);
void* thr_cleanup(unsigned char* d, int fd);
char* get_user_list();

typedef struct pthread_arg_t 
{
  int sockfd;
  struct sockaddr_in newAddr;
  char *user; 
  int fd;
} pthread_arg_t;

// To create array of clients with relevant info
typedef struct client_info {
  int fd;
  char *name;
  int online;
  struct sockaddr_in addr; 
}client_info;

// Holds data to be transmitted in I/O threads
typedef struct msg_data {
  int fd;
  struct data_pkt *pkt;
  struct cls_pkt *cp;
  struct init_pkt *init;
}msg_data;

typedef struct epoll_task {
  epoll_data_t data;
  struct epoll_task *next;
}epoll_task;

int get_clientfd(char *user_to_find);
int epoll_fd, ready;
struct epoll_event ev;
struct epoll_event events[MAXUSERS];
epoll_task *to_read = NULL, *read_tail = NULL;
epoll_task *to_write = NULL, *write_tail = NULL;
pthread_t read_thr, write_thr, connect_thr;
pthread_cond_t read_cond, write_cond;
pthread_mutex_t read_lock, write_lock, lock, msg_lock;

int sockfd, newSocket, num_users;
struct sockaddr_in serverAddr, newAddr;
struct init_pkt *p;
socklen_t len;
struct client_info *clients;
bool validate_input(char *a);

void read_queue_add(epoll_task *add)
{
  if(!add)
    return;
  if(!to_read){
    to_read = add;
    read_tail = add;
  }
  else{
    read_tail->next = add;
    read_tail = add;
  }
}

void write_queue_add(epoll_task *add)
{
  if(!add)
    return;
  if(!to_write){
    to_write = add;
    write_tail = add;
  }
  else{
    write_tail->next = add;
    write_tail = add;
  }
}

int set_nonblocking(int fd)
{
  int arg;
  arg = fcntl(fd, F_GETFL);
  if(arg < 0){
    perror("fcntl error with F_GETFL\n");
    return -1;
  }
  arg |= O_NONBLOCK;
  if(fcntl(fd, F_SETFL, arg) < 0){
    perror("fcntl error with F_SETFL");
    return -1;
  }
  ev.data.fd = fd;
  ev.events = EPOLLIN | EPOLLET;
  if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0){ // Add to interest list for reading:w
    perror("epoll_ctl error: 455");
    return -1;
  }
  return 0;
}

// Adds a new user, malloc'ing and populating relevant fields and incremented number of connected users
int new_user(char *name, int fd)
{
  if(pthread_mutex_trylock(&lock) != EBUSY){
    printf("new_user must be holding lock\n");
    return -1;
  }

  if(!name)
    return -1;

  if(!clients)
    clients = malloc(MAXUSERS*sizeof(struct client_info));

  if(num_users == MAXUSERS){ 
    printf("Maximum users connected, cannot connect at this time\n");
    return -1;
  }

  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i) || !clients[i].name) break;
    if(!strcmp(clients[i].name, name) && (get_clientfd(name) != -1)){
      if(clients[i].online){
        printf("%s already taken. Please enter another username: ", name);
        return 0;
      }
      else{
        clients[i].online = 1;
        if(clients[i].fd != fd){ // if fd is different from before
          if(set_nonblocking(fd) == -1){
            printf("error setting fd %d as nonblocking\n", fd);
            return -1;
          }
          clients[i].fd = fd;
        }
        return i;
      }
    }
  }

  clients[num_users].name = malloc(strlen(name)+1);
  if(!clients[num_users].name){
    perror("malloc for new client name failed");
    return -1;
  }
  memcpy(clients[num_users].name, name, strlen(name));
  clients[num_users].fd = fd;
  if(set_nonblocking(fd) == -1){
    printf("error setting fd %d as nonblocking\n", fd);
    return -1;
  }
  clients[num_users].online = 1;
  printf("New client: %s fd: %d\n", clients[num_users].name, clients[num_users].fd);
  ++num_users;

  return 0; 
}

// Broadcasts message to all online users
int send_to_all(struct data_pkt *pkt)
{
  if(pthread_mutex_trylock(&msg_lock) != EBUSY){
    printf("must be holding msg_lock\n");
    return -1;
  }
  int n;
  char msg[1024];

  if(!pkt || !pkt->dst || !pkt->data)
    return -1;

  memset(msg, '\0', 1024);
  strncpy(msg, pkt->src, strlen(pkt->src));
  strcat(msg, ": ");
  strcat(msg, pkt->data);
  struct data_pkt msg_pkt;
  msg_pkt.type = DATA;
  strcpy(msg_pkt.src, pkt->src);
  strcpy(msg_pkt.data, msg);
  msg_pkt.id = 1;
  char *u = ser_data(&msg_pkt, DATA);
  char *data = hide_zeros((unsigned char*)u);
  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i)) break; // Avoids seg fault
    if(!clients[i].online) continue;
    n = send(clients[i].fd, data, 1024, 0);
    printf("msg %s sent to %s\n", msg, clients[i].name);
    if(n > 0){
      ev.data.fd = clients[i].fd;
      ev.events = EPOLLIN | EPOLLET;
      if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clients[i].fd, &ev) < 0)
        perror("epoll_ctl error: 192");
    }
  }
  return 0;
} 

// Sends a message to an individual client matching the send_to_fd argument
int send_msg(struct data_pkt *pkt, int send_to_fd)
{
  if(pthread_mutex_trylock(&msg_lock) != EBUSY){
    printf("must be holding msg_lock\n");
    return -1;
  }

  int n;
  char msg[1024];

  if(!pkt || !pkt->dst || !pkt->data)
    return -1;

  for(int i = 0; i < MAXUSERS; ++i) {
    if(!(clients+i)) break; // avoids seg fault
    if(!clients[i].online) continue;
    if(!strncmp(pkt->dst, clients[i].name, strlen(pkt->dst))){
      memset(msg, '\0', 1024);
      strncpy(msg, pkt->src, strlen(pkt->src));
      strcat(msg, ": ");
      strcat(msg, pkt->data);
      struct data_pkt msg_pkt;
      msg_pkt.type = DATA;
      strcpy(msg_pkt.src, pkt->src);
      strcpy(msg_pkt.dst, pkt->dst);
      strcpy(msg_pkt.data, msg);
      msg_pkt.id = 1;
      char *u = ser_data(&msg_pkt, DATA);
      char *data = hide_zeros((unsigned char*)u);
      n = send(send_to_fd, data, 1024, 0);
      printf("%s sent to %s\n", msg_pkt.data, msg_pkt.dst);
      if(n > 0){
        ev.data.fd = send_to_fd;
        ev.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, send_to_fd, &ev) < 0){
          perror("epoll_ctl error: 234");
          return -1;
        }
        return 0;
      }
      printf("error sending message to %s\n", msg_pkt.dst);
      return -1;
    }
  }
  printf("user %s not found\n", pkt->dst);
  return -1;
}

// Sends a message directly from the server to a client
int server_to_client_msg(int fd, struct init_pkt *pkt, char *msg)
{
  if(!pkt || !msg || fd < 0)
    return -1;

  struct data_pkt msg_pkt;
  msg_pkt.type = DATA;
  strcpy(msg_pkt.src, p->dst);
  strcpy(msg_pkt.dst, p->src);
  strcpy(msg_pkt.data, msg);
  msg_pkt.id = 1;
  char *u = ser_data(&msg_pkt, DATA);
  char *user_data = hide_zeros((unsigned char*)u);
  int n = send(fd, user_data, 1024, 0);

  if(n > 0)
    return 0;

  if(n < 0){
    perror("send error: ");
    return -1;
  }

  if(!n){
    close(fd);
    printf("client closed connection\n");
    return -1;
  }
  return -1;
}

// Returns the fd of the client whose username matches the argument
int get_clientfd(char *user_to_find)
{
  if(!user_to_find)
    return -1;

  if(!strcmp(user_to_find, server_name))
    return 0;

  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i)) break; 
    if(!clients[i].online) continue;
    if(!strncmp(user_to_find, clients[i].name, strlen(user_to_find)) && clients[i].online)
      return clients[i].fd;
  }

  return -1;
}

int is_valid_fd(int fd)
{
  if(fd < 0){
    printf("invalid fd %d\n", fd);
    return -1;
  }

  for(int i = 0; i < MAXUSERS; ++i){
    if(clients[i].fd == fd)
      return clients[i].online;
  }

  printf("invalid fd %d\n", fd);
  return -1;
}

int main(int argc, char *argv[])
{
  connection_info connection;

  signal(SIGINT,INThandler);
  if (argc!=2)
  { fprintf (stderr, "Usage: %s <port>\n", argv[0]);
    exit (EXIT_FAILURE);
  }
  if (validate_input(argv[1])){
    pthread_mutex_init(&read_lock, 0);
    pthread_mutex_init(&write_lock, 0);
    pthread_mutex_init(&lock, 0);
    pthread_mutex_init(&msg_lock, 0);
    pthread_cond_init(&read_cond, 0);
    pthread_cond_init(&write_cond, 0);
    startup(&connection,atoi(argv[1]));
    pthread_create(&read_thr, 0, do_reads, 0);
    pthread_create(&write_thr, 0, do_writes, 0);
    pthread_create(&connect_thr, 0, accept_conn, (void*)&connection);
  }
  else {
    perror("not a valid port number");
    exit(EXIT_FAILURE);
  }

  pthread_join(connect_thr, 0);
  pthread_join(read_thr, 0);
  pthread_join(write_thr, 0);
  printf("Server has disconnected\n");
  exit(EXIT_SUCCESS);
}

/* Starting routine for connection thread:
   Calls epoll_wait to identify fds ready for activity, then sets up necessary action */
void* accept_conn(void *arg)
{
  unsigned char *d = malloc(1024);
  char *u;
  int n, fd;
  connection_info *connection = (struct connection_info*)arg;
  char data[1024];

  for(;;)
  {
    if(num_users == -1)
      break;

    if(!(ready = epoll_wait(epoll_fd, events, MAXUSERS, TIMEOUT)))
      continue;

    for(int i = 0; i < ready; ++i){
      // A client is trying to connect to the server socket
      if(events[i].data.fd == connection->sockfd) {
        len = sizeof(connection->serverAddr);
        fd = accept(connection->sockfd, (struct sockaddr*)&connection->serverAddr, &len);
        if(fd == EAGAIN){
          printf("already added %d", fd);
          continue;
        }
        n = recv(fd, data, 1024, 0);
        if(n < 0){
          if(errno == ECONNRESET)
            close(fd);
          perror("error receiving init packet for new connection");
          continue;
        }
        else if(!n){
          close(fd);
          printf("client closed connection\n");
          continue;
        }

        printf("bytes received %d\n", n);
        memcpy(d, data, n);
        d[n] = '\0';
        u = unhide_zeros(d);

        // Init packet has been received, server sends back ack packet
        if(u[0] == 0x01){
          p = deser_init_pkt(u);
          pthread_mutex_lock(&lock);
          struct ack_pkt ack;
          ack.type = ACK;
          ack.id = 1;
          strcpy(ack.src, server_name);
          strcpy(ack.dst, p->src);
          char *serack = ser_data(&ack, ACK);
          char *udata = hide_zeros((unsigned char*)serack);
          if((n = send(fd, udata, strlen(udata), 0)) < 0){
            perror("error sending ACK packet:");
            close(fd);
            continue;
          }
          if(!n){
            printf("client disconnected\n");
            close(fd);
            continue;
          }
          char *user_list;
          while(!(user_list = get_user_list())) ; // Only will return null if not holding lock, shouldn't happen
          if(server_to_client_msg(fd, p, user_list)){
            free(user_list);
            pthread_mutex_unlock(&lock);
            printf("error sending message to client %s\n", p->src);
            continue;
          }
          free(user_list);
          int ret = new_user(p->src, fd);
          if(!ret){
            printf("User %s has connected on fd %d\n", p->src, fd); 
            struct data_pkt *pkt = malloc(sizeof(struct data_pkt));
            pkt->type = DATA;
            pkt->id = 1;
            strcpy(pkt->dst, msg_to_all);
            strcpy(pkt->src, p->src);
            strcpy(pkt->data, " has entered the chat\n");
            pthread_mutex_lock(&msg_lock);
            send_to_all(pkt);
            free(pkt);
            pthread_mutex_unlock(&msg_lock);
          }
          /*  else if(ret == 1)
              continue;
          // PROMPT FOR ANOTHER NAME*/
          else{
            pthread_mutex_unlock(&lock);
            continue;
          }
          pthread_mutex_unlock(&lock);

        }
      }
      // fd is ready to be read from
      else if(events[i].events & EPOLLIN && is_valid_fd(events[i].data.fd)){
        epoll_task *task = malloc(sizeof(struct epoll_task));
        task->data.fd = events[i].data.fd;
        task->next = NULL;
        pthread_mutex_lock(&read_lock);
        read_queue_add(task);
        pthread_cond_broadcast(&read_cond);
        pthread_mutex_unlock(&read_lock);
      } 
      // fd is ready to be written to
      else if(events[i].events & EPOLLOUT && is_valid_fd(events[i].data.fd)){
        if(!events[i].data.ptr)
          continue;
        epoll_task *task = malloc(sizeof(struct epoll_task));
        task->data.ptr = (struct msg_data*)events[i].data.ptr;
        task->next = NULL;
        pthread_mutex_lock(&write_lock);
        write_queue_add(task);
        pthread_cond_broadcast(&write_cond);
        pthread_mutex_unlock(&write_lock);
      }  
      else
        perror("something went wrong");
    }
  }

  pthread_cancel(read_thr);
  pthread_cancel(write_thr);
  printf("Server exiting...\n");
  return thr_cleanup(d, connection->sockfd);
}

void do_on_exit(int fd)
{
  struct data_pkt *pkt = malloc(sizeof(struct data_pkt));

  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i)) break;
    if(!clients[i].online) continue;
    if(clients[i].fd == fd){
      clients[i].online = 0;
      close(clients[i].fd);
      printf("Disconnected %s:%d\n", inet_ntoa(clients[i].addr.sin_addr), ntohs(clients[i].addr.sin_port));
      --num_users;
      if(!num_users) --num_users; // If only active user, decrement to -1 so server exits
      pkt->type = DATA;
      pkt->id = 1;
      strcpy(pkt->dst, msg_to_all);
      strcpy(pkt->src, clients[i].name);
      strcpy(pkt->data, " has exited the chat\n");
      pthread_mutex_lock(&msg_lock);
      send_to_all(pkt);
      free(pkt);
      pthread_mutex_unlock(&msg_lock);
      break;
    }
  }
}

void* do_reads(void *arg)
{
  msg_data *msg_data = NULL;
  int n;
  unsigned char *d = malloc(1024);
  char *u;
  char data[1024];

  for(;;){
    pthread_mutex_lock(&read_lock);
    while(!to_read)
      pthread_cond_wait(&read_cond, &read_lock); // Wait until ready to read and woken by connect_thr
    int fd = to_read->data.fd;
    epoll_task *task = to_read;
    to_read = to_read->next;
    free(task);
    pthread_mutex_unlock(&read_lock);
    n = recv(fd, data, 1024, 0);
    if(n < 0){
      if(n == EBADF) // Means user has exited 
        continue;
      if(errno == ECONNRESET)
        close(fd);
      printf("error reading from fd %d\n", fd);
      continue;
    }
    else if(!n){
      printf("client on fd %d closed connection\n", fd);
      pthread_mutex_lock(&lock);
      do_on_exit(fd);
      pthread_mutex_unlock(&lock);
      continue;
    }

    memcpy(d, data, n);
    d[n] = '\0';
    u = unhide_zeros(d);

    if(u[0] == 0x01) continue; // Already received init packet in connect_thr, shouldn't receive another

    msg_data = malloc(sizeof(struct msg_data));
    msg_data->fd = fd;

    if(u[0] == 0x03) // Received msg from client
    {
      msg_data->pkt = (struct data_pkt*)deser_data_pkt(u);
      if(!strcmp(msg_data->pkt->data, ":exit"))
      {
        pthread_mutex_lock(&lock);
        do_on_exit(fd);
        pthread_mutex_unlock(&lock);
        free(msg_data);
        continue;
      }
      if(strcmp(msg_data->pkt->dst, server_name)){ // Message to a particular client
        msg_data->fd = get_clientfd(msg_data->pkt->dst);
        if(fd == -1){
          printf("User %s not available", msg_data->pkt->dst);
          free(msg_data);
          continue;
        }
        ev.data.ptr = msg_data; // Contains message packet
        ev.events = EPOLLOUT | EPOLLET;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, msg_data->fd, &ev) < 0) // Change fd for destination client from read list to write list
          perror("epoll_ctl error:");
      }
      else {   // Message to all
        ev.data.ptr = msg_data;
        ev.events = EPOLLOUT | EPOLLET;
        for(int i = 0; i < MAXUSERS; ++i){
          if(!clients[i].online) continue;
          if(!strcmp(clients[i].name, msg_data->pkt->src)) {
            if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clients[i].fd, &ev) < 0)
              perror("epoll_ctl error:");
          }
        }
      }
    }
    else if(u[0] == 0x04){ // Close packet
      pthread_mutex_lock(&lock);
      do_on_exit(msg_data->fd);
      free(msg_data);
      pthread_mutex_unlock(&lock);
    }
      
    bzero(data, sizeof(data));
  }
  printf("read thread shutting down...\n");
  if(msg_data) free(msg_data);
  if(d) free(d);
  return 0;
}

void* do_writes(void *arg)
{
  int fd;
  msg_data *msg_data = NULL;

  for(;;){
    pthread_mutex_lock(&write_lock);
    while(!to_write)
      pthread_cond_wait(&write_cond, &write_lock);
    msg_data = (struct msg_data*)to_write->data.ptr;
    if(!msg_data)
      continue;
    epoll_task *task = to_write;
    to_write = to_write->next;
    free(task);
    pthread_mutex_unlock(&write_lock);
    pthread_mutex_lock(&msg_lock);
    printf("msg: %s\n", msg_data->pkt->data);
    if(!strcmp(msg_data->pkt->dst, server_name)){
      if(send_to_all(msg_data->pkt))
        printf("error broadcasting message\n");
      pthread_mutex_unlock(&msg_lock);
      free(msg_data); // Don't need after message successfully sent
      continue;
    }

    fd = get_clientfd(msg_data->pkt->dst);
    if(send_msg(msg_data->pkt, fd)) 
      printf("error sending msg %s from %s to %s\n", msg_data->pkt->data, msg_data->pkt->src, msg_data->pkt->dst);
    pthread_mutex_unlock(&msg_lock);
    free(msg_data);
  }
  printf("write thread shutting down...\n");
  if(msg_data) free(msg_data);
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

  // Create epoll instance and making server socket fd non-blocking
  epoll_fd = epoll_create(MAXUSERS*2+1); // Enough for read and write for each user plus 1 for server fd
  if(set_nonblocking(connection->sockfd) == -1){
    printf("[-] Error setting server socket fd as nonblocking\n");
    exit(1);
  }
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
  if(listen(connection->sockfd, MAXPENDING)==0)
  {
    printf("[+]Listening...\n");
  }else
  {
    printf("[-]Error in binding.\n");
  }
}

void* thr_cleanup(unsigned char *d, int fd)
{
  if(clients){
    for(int i = 0; i < MAXUSERS; ++i){
      if(clients[i].name) free(clients[i].name);
      close(clients[i].fd);
    }
    free(clients);
  }

  if(fd) close(fd); // Only will happen once for socket fd
  if(d) free(d);
  return 0;
}

// Sends user list to user upon connecting and when requested
char* get_user_list()
{
  if(pthread_mutex_trylock(&lock) != EBUSY){
    printf("get_user_list must be holding lock\n");
    return 0;
  }
  int num = 1;
  char *user_list = malloc(1024);
  memset(user_list, '\0', 1024);

  if(!num_users){
    strcpy(user_list, "No other users currently online\n");
    return user_list;
  }

  strcpy(user_list, "Online Users: ");
  int len = strlen(user_list);
  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i)) break; // Avoids seg fault
    if(!clients[i].online) continue;
    len += sprintf(user_list+len, "\n%d) %s", num++, clients[i].name);
  }
  strcat(user_list, "\n");
  return user_list;
}

bool validate_input(char *a)
{
  int i=0;
  if (a[i] == '-') i=1; //negative number	
  for(;a[i]!='\0';i++)
  {
    if(!isdigit(a[i]))
      return false;
  } 
  return true;	
}
