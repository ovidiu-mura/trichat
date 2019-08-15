/* CS510 - ALSP
 * Team: Alex Davidoff, Kamakshi Nagar, Ovidiu Mura
 * Date: 08/13/2019
 *
 * Trichat server openes an socket and starts listenning multiple clients.
 * When the clients send data to server, the packets are processed and sent
 * to the destination user using a different thread. Data received by each user fd
 * is processed using advanced io technique 'epoll' which monitor the fds
 * for data arrived in the ready fds. There is a daemon during the server
 * lifetime which logs messages into the local syslog.
 * Start the server command: ./server <port>
 * */

#include "libs.h"
#define MAXPENDING 10
#define MAXUSERS 20
#define TIMEOUT 1000

static const char *server_name = ">>server**";
static const char *msg_to_all = ">>broadcast**";

void server_log(char*);
pthread_mutex_t llock;

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
int server_to_client_msg(int, char*, char*);

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
  if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0){ // Add to interest list for reading
    perror("epoll_ctl error");
    return -1;
  }
  return 0;
}

// Adds a new user, malloc'ing and populating relevant fields and incremented number of connected users
int new_user(char *name, int fd, struct sockaddr_in cAddr)
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
    server_to_client_msg(fd, "Maximum users connected, cannot connect at this time\n", name);
    return -1;
  }

  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i) || !clients[i].name) break;
    if(!strcmp(clients[i].name, name) && (get_clientfd(name) != -1)){
      if(clients[i].online){
        sprintf(name, "%s%d", name, num_users);
        char msg[100];
        sprintf(msg, "Username %s already exists. Logging you in as %s", clients[i].name, name);
        server_to_client_msg(fd, msg, name);
      }
      else {
        clients[i].online = 1;
        if(clients[i].fd != fd){ // if fd is different from before
          if(set_nonblocking(fd) == -1){
            printf("error setting fd %d as nonblocking\n", fd);
            return -1;
          }
          clients[i].fd = fd;
        }
        return 0;
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

  memcpy(&clients[num_users].addr, &cAddr, sizeof(cAddr));
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
        perror("epoll_ctl error");
    }
  }
  return 0;
} 

// It checks if a fd is valid
int is_valid_fd(int fd)
{
  if(fd < 0){
    return -1;
  }

  for(int i = 0; i < MAXUSERS; ++i){
    if(!(clients+i) || !clients[i].name) break;
    if(clients[i].fd == fd)
      return clients[i].online;
  }

  return -1;
}

// Sends a message to an individual client matching the send_to_fd argument
int send_msg(struct data_pkt *pkt, int send_to_fd)
{
  if(pthread_mutex_trylock(&msg_lock) != EBUSY){
    printf("must be holding msg_lock\n");
    return -1;
  }
  int n;
  char tmp[500];
  char msg[1024];
  if(!pkt || !pkt->dst || !pkt->data)
    return -1;

  memset(msg, '\0', 1024);
  if(!is_valid_fd(send_to_fd)){ // Client to send to has disconnected
    send_to_fd = get_clientfd(pkt->src);
    sprintf(msg, "Could not send %s -- User %s has exited", pkt->data, pkt->dst);
    strcpy(pkt->dst, pkt->src);
    strcpy(pkt->src, server_name);
  }
  else{
    strncpy(msg, pkt->src, strlen(pkt->src));
    strcat(msg, ": ");
    strcat(msg, pkt->data);
  }
  for(int i = 0; i < MAXUSERS; ++i) {
    if(!(clients+i)) break; // avoids seg fault
    if(!clients[i].online) continue;
    if(!strncmp(pkt->dst, clients[i].name, strlen(pkt->dst))){
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
      sprintf(tmp, "%s sent to %s", msg_pkt.data, msg_pkt.dst);
      server_log(tmp);
      if(n > 0){
        ev.data.fd = send_to_fd;
        ev.events = EPOLLIN | EPOLLET;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, send_to_fd, &ev) < 0){
          perror("epoll_ctl error");
          return -1;
        }
        return 0;
      }
      printf("error sending message to %s\n", msg_pkt.dst);
      sprintf(tmp, "error sending message to %s", msg_pkt.dst);
      server_log(tmp);
      return -1;
    }
  }
  printf("user %s not found\n", pkt->dst);
  return -1;
}

// Sends a message directly from the server to a client
int server_to_client_msg(int fd, char *msg, char *cli)
{
  if(!msg || fd < 0)
    return -1;

  struct data_pkt msg_pkt;
  msg_pkt.type = DATA;
  strcpy(msg_pkt.src, server_name);
  strcpy(msg_pkt.dst, cli);
  strcpy(msg_pkt.data, msg);
  if(!strncmp(msg, "Username ", 9))
    msg_pkt.id = 2;
  else
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
    if(!(clients+i) || !clients[i].name) break; 
    if(!strcmp(user_to_find, clients[i].name))
      return clients[i].fd;
  }

  return -1;
}

void start_log_daemon()
{
  ptr = create_sm(1024);
  strcpy(ptr, "trichat server started");
  key_t shk = ftok("server.c", 'b');
  shmid = shmget(shk, 20, 0666|IPC_CREAT);
  shmp = shmat(shmid, NULL, 0);
  shmp[1] = 55555;
  pid_t pid = fork();
  if(pid == 0)
  {
    create_daemon();
  }
  while(shmp[1] != 33333);
  kill(shmp[2], SIGUSR1);
  pid_t pd = shmp[2];
  wait(&pd);
}

void server_log(char *msg)
{
  pthread_mutex_lock(&llock);
  strncpy(ptr, msg, strlen(msg));
  ptr[strlen(msg)] = '\0';
  pthread_mutex_unlock(&llock);
  kill(shmp[2], SIGUSR1);
}

void stop_log_daemon()
{
  kill(shmp[2], SIGKILL);
  shmdt(shmp);
  shmctl(shmid, IPC_RMID, 0);
  munmap(ptr, 1024);
}

int main(int argc, char *argv[])
{
  start_log_daemon();
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
    pthread_create(&read_thr, 0, do_reads, (void*)&connection);
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
  stop_log_daemon();
  exit(EXIT_SUCCESS);
}

/* Starting routine for connection thread:
   Calls epoll_wait to identify fds ready for activity, then sets up necessary action */
void* accept_conn(void *arg)
{
  unsigned char *d = malloc(1024);
  char *u, *user_list;
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
        char *ipv4 = inet_ntoa((connection->serverAddr).sin_addr);
        char por[10];
        sprintf(por, "%d", ntohs((connection->serverAddr).sin_port));
        char *msg = strcat(strcat(ipv4, ":"), por);
        msg[21] = '\0';
        server_log(msg);
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
        } else if(!n){
          close(fd);
          printf("client closed connection\n");
          continue;
        }
        char tt[50];
        sprintf(tt, "first %d bytes received", n);
        server_log(tt);
        memcpy(d, data, n);
        d[n] = '\0';
        u = unhide_zeros(d);

        // Init packet has been received, server verifies user can be added to list sends back ack packet
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

          while(!(user_list = get_user_list())) ; // Only will return null if not holding lock, shouldn't happen
          if(server_to_client_msg(fd, user_list, p->src)){
            free(user_list);
            pthread_mutex_unlock(&lock);
            printf("error sending message to client %s\n", p->src);
            continue;
          }
          free(user_list);
          int ret = new_user(p->src, fd, connection->serverAddr);
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
      else if(events[i].events & EPOLLOUT){
        if(!events[i].data.ptr) continue;
        epoll_task *task = malloc(sizeof(struct epoll_task));
        task->data.ptr = (struct msg_data*)events[i].data.ptr;
        task->next = NULL;
        pthread_mutex_lock(&write_lock);
        write_queue_add(task);
        pthread_cond_broadcast(&write_cond);
        pthread_mutex_unlock(&write_lock);
      }  
      else {
        perror("something went wrong");
        server_log("something went wrong");
      }
    }
  }

  pthread_cancel(read_thr);
  pthread_cancel(write_thr);
  printf("Server exiting...\n");
  server_log("Server exiting...");
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
      char te[100];
      sprintf(te, "Disconnected %s:%d", inet_ntoa(clients[i].addr.sin_addr), ntohs(clients[i].addr.sin_port));
      server_log(te);
      --num_users;
      if(!num_users) --num_users; // If only active user, decrement to -1 so server exits
      pkt->type = DATA;
      pkt->id = 1;
      strcpy(pkt->dst, msg_to_all);
      strcpy(pkt->src, clients[i].name);
      strcpy(pkt->data, " has exited the chat\n");
      strcpy(te, strcat(pkt->src, " has exited the chat\0"));
      server_log(te);
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
    } else if(!n){
      printf("client on fd %d closed connection\n", fd);
      pthread_mutex_lock(&lock);
      do_on_exit(fd);
      pthread_mutex_unlock(&lock);
      continue;
    }

    memcpy(d, data, n);
    d[n] = '\0';
    u = unhide_zeros(d);

    if(u[0] == 0x01) continue;

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

      if(!strcmp(msg_data->pkt->data, ":users"))
      {
        char *user_list;
        pthread_mutex_lock(&lock);
        while(!(user_list = get_user_list())) ; // Only will return null if not holding lock, shouldn't happen
        if(server_to_client_msg(fd, user_list, msg_data->pkt->src)){
          free(user_list);
          pthread_mutex_unlock(&lock);
          printf("error sending message to client %s\n", msg_data->pkt->src);
          continue;
        } 
        free(user_list);
        pthread_mutex_unlock(&lock);
        continue;
      }

      if(strcmp(msg_data->pkt->dst, server_name)){ // Message to a particular client
        msg_data->fd = get_clientfd(msg_data->pkt->dst);
        if(!is_valid_fd(msg_data->fd)){
          msg_data->fd = get_clientfd(msg_data->pkt->src);
          char msg[1024];
          sprintf(msg, "Could not send %s -- User %s has exited", msg_data->pkt->data, msg_data->pkt->dst);
          strcpy(msg_data->pkt->data, msg);
          strcpy(msg_data->pkt->dst, msg_data->pkt->src);
          strcpy(msg_data->pkt->src, server_name);
        }
        if(msg_data->fd == -1){
          free(msg_data);
          continue;
        }
        ev.data.ptr = msg_data; // Contains message packet
        ev.events = EPOLLOUT | EPOLLET;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, msg_data->fd, &ev) < 0) // Change fd for destination client from read list to write list
          perror("epoll_ctl error");
      }
      else {   // Message to all
        ev.data.ptr = msg_data;
        ev.events = EPOLLOUT | EPOLLET;
        for(int i = 0; i < MAXUSERS; ++i){
          if(!clients[i].online) continue;
          if(!strcmp(clients[i].name, msg_data->pkt->src)) {
            if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, clients[i].fd, &ev) < 0)
              perror("epoll_ctl error");
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
  server_log("read thread shutting down...\0");
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
  } else
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
  server_log(user_list);
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
