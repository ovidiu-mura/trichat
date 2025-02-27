/* CS510 - ALSP
 * Team: Alex Davidoff, Kamakshi Nagar, Ovidiu Mura
 * Date: 08/13/2019
 *
 * It implements serealize, de-serialize functionality the app
 * to be able to send data on the connection. A daemon which 
 * starts when the server starts and closes when the server terminates, 
 * the daemon logs messages to the local syslog.
 **/

#include "libs.h"

unsigned char key[512];

// It serialize data for the packet type.
char * ser_data(void *pkt, char tp)
{
  char *ser;
  int off=0;
  if(tp == INIT)
  {
    ser = malloc(sizeof(struct init_pkt));
    memcpy(ser, &((struct init_pkt*)pkt)->type, sizeof(char));
    off = sizeof(char);
    memcpy(ser+off, &((struct init_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off, &((struct init_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off, &((struct init_pkt*)pkt)->dst, sizeof(char)*100);
  } 
  else if(tp == ACK)
  {
    ser = malloc(sizeof(struct ack_pkt));
    memcpy(ser, &((struct ack_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off, &((struct ack_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off, &((struct ack_pkt*)pkt)->src, sizeof(char)*20);
    off += sizeof(char)*20;
    memcpy(ser+off, &((struct ack_pkt*)pkt)->dst, sizeof(char)*20);
  }
  else if(tp == DATA)
  {
    ser = malloc(sizeof(struct data_pkt));
    memcpy(ser, &((struct data_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off, &((struct data_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off, &((struct data_pkt*)pkt)->data, sizeof(char)*467);
    off += sizeof(char)*467;
    memcpy(ser+off, &((struct data_pkt*)pkt)->src, sizeof(char)*20);
    off += sizeof(char)*20;
    memcpy(ser+off, &((struct data_pkt*)pkt)->dst, sizeof(char)*20);
  }
  else if(tp == CLS)
  {
    ser = malloc(sizeof(struct cls_pkt));
    memcpy(ser, &((struct cls_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off, &((struct cls_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off, &((struct cls_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off, &((struct cls_pkt*)pkt)->dst, sizeof(char)*100);
  }
  return ser;
}

// It de-serializes data.
struct init_pkt* deser_init_pkt(char *ptr)
{
  char *tmp = (char*)ptr;
  struct init_pkt* p = malloc(sizeof(struct init_pkt));
  memcpy(&((struct init_pkt*)p)->type, tmp, 1);
  memcpy(&((struct init_pkt*)p)->id, tmp+1,4);
  memcpy(&((struct init_pkt*)p)->src, tmp+5, 100);
  memcpy(&((struct init_pkt*)p)->dst, tmp+105, 100);

  return p;
}

// It de-serializes the data packet.
struct data_pkt* deser_data_pkt(char *ptr)
{
  char *tmp = (char*)ptr;
  struct data_pkt* p = malloc(sizeof(struct data_pkt));
  memcpy(&((struct data_pkt*)p)->type, tmp, 1);
  memcpy(&((struct data_pkt*)p)->id, tmp+1, 4);
  memcpy(&((struct data_pkt*)p)->data, tmp+5, 467);
  memcpy(&((struct data_pkt*)p)->src, tmp+472, 20);
  memcpy(&((struct data_pkt*)p)->dst, tmp+492, 20);
  return p;
}

// It de-serializes the ack packet.
struct ack_pkt* deser_ack_pkt(char *ptr)
{
  char *tmp = (char*)ptr;
  struct ack_pkt* p = malloc(sizeof(struct ack_pkt));
  memcpy(&((struct ack_pkt*)p)->type, tmp, 1);
  memcpy(&((struct ack_pkt*)p)->id, tmp+1, 4);
  memcpy(&((struct ack_pkt*)p)->src, tmp+5, 20);
  memcpy(&((struct ack_pkt*)p)->dst, tmp+25, 20);
  return p;
}

// It de-serializes the cls packet.
struct cls_pkt* deser_cls_pkt(char *ptr)
{
  char *tmp = (char*)ptr;
  struct cls_pkt* p = malloc(sizeof(struct cls_pkt));
  memcpy(&((struct cls_pkt*)p)->type, tmp, 1);
  memcpy(&((struct cls_pkt*)p)->id, tmp+1, 4);
  memcpy(&((struct cls_pkt*)p)->src, tmp+5, 100);
  memcpy(&((struct cls_pkt*)p)->dst, tmp+105, 100);
  return p;
}

// It de-serializes the data packet.
char * deser_data(void *pkt)
{
  char *deser;
  char *tmp = (char*)pkt;

  if(tmp[0]==INIT)
  { 
    deser = malloc(sizeof(struct init_pkt));
    memcpy(deser, &((struct init_pkt*)pkt)->type, sizeof(char));
    memcpy(deser+1, &((struct init_pkt*)pkt)->id, sizeof(int));
    memcpy(deser+5, &((struct init_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct init_pkt*)deser)->dst, &((struct init_pkt*)pkt)->dst, sizeof(char)*100);
  }
  else if(tmp[0] == ACK)
  {
    deser = malloc(sizeof(struct ack_pkt));
    memcpy(&((struct ack_pkt*)pkt)->type, &((struct ack_pkt*)pkt)->type, sizeof(char));
    memcpy(&((struct ack_pkt*)pkt)->id, &((struct ack_pkt*)pkt)->id, sizeof(int));
    memcpy(&((struct ack_pkt*)pkt)->src, &((struct ack_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct ack_pkt*)pkt)->dst, &((struct ack_pkt*)pkt)->dst, sizeof(char)*100);
  }
  else if(tmp[0] == DATA)
  { 
    deser = malloc(sizeof(struct data_pkt));
    memcpy(&((struct data_pkt*)deser)->type, &((struct data_pkt*)pkt)->type, sizeof(char));
    memcpy(&((struct data_pkt*)deser)->id, &((struct data_pkt*)pkt)->id, sizeof(int));
    memcpy(&((struct data_pkt*)deser)->src, &((struct data_pkt*)pkt)->src, sizeof(char)*20);
    memcpy(&((struct data_pkt*)deser)->dst, &((struct data_pkt*)pkt)->dst, sizeof(char)*20);
    memcpy(&((struct data_pkt*)deser)->data, &((struct data_pkt*)pkt)->data, sizeof(char)*467);
  }
  else if(tmp[0] == CLS)
  {
    deser = malloc(sizeof(struct cls_pkt));
    memcpy(&((struct cls_pkt*)deser)->type, &((struct cls_pkt*)pkt)->type, sizeof(char));
    memcpy(&((struct cls_pkt*)deser)->id, &((struct cls_pkt*)pkt)->id, sizeof(int));
    memcpy(&((struct cls_pkt*)deser)->src, &((struct cls_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct cls_pkt*)deser)->dst, &((struct cls_pkt*)pkt)->dst, sizeof(char)*100);
  }
  else
  {
    printf("PACKET TYPE DOES NOT EXIST!!!\n");
  }
  return deser;
}

// It hides the 0x00 values in the packet if they appear in the first 512 bytes.
char * hide_zeros(unsigned char *ptr)
{
  for(int i=0;i<512; i++)
	  key[i] = 0xAA;
  char *data = malloc(1024);
  for(int i =0; i<512; i++)
  {
    if(ptr[i] == 0x0)
    {
      ptr[i] = 0xFF;
      key[i] = 0xFF;
    }
    memcpy(data+i, ptr+i, 1);
  }
  for(int j=512; j<1024; j++)
  {
    memcpy(data+j, &key[j-512], 1);
  }
  data[1024] = 0x0;
  return data;
}

// It unhides the 0x00 values in the packet if they appear in the first 512 bytes.
char * unhide_zeros(unsigned char *ptr)
{
  char *data = malloc(1024);
  for(int i=0; i<512; i++)
  {
    if(ptr[i+512] == 0xFF)
    {
      ptr[i] = 0x0;
    }
    memcpy(data+i, ptr+i, 1);
  }
  data[1024] = 0x0;
  return data;
}

// It terminates the daemon.
void kill_daemon()
{
  exit(0);
}

// It creates a daemon to log the messages from shared memory to local syslog.
void create_daemon()
{
    pid_t pid;

    /* fork the parent process */
    pid = fork();

    /* check if error occurred in child */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* the child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* catch, ignore and handle signals */
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    act.sa_handler = sigusr1;
    sigaction(SIGUSR1, &act, NULL);
    signal(SIGKILL, kill_daemon);
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* fork second time*/
    pid = fork();

    /* check if error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* set file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>=0; x--)
    {
        close (x);
    }
    setlogmask(LOG_UPTO(LOG_DEBUG));
    /* Open the log file, /var/log/messages.local.log */
    openlog ("trichat", LOG_CONS|LOG_PID|LOG_NDELAY, LOG_LOCAL1);
    pid = getpid();
    shmp[2] = pid;
    shmp[1] = 33333;
    while(1)
      pause();
}

// It creates a shared memory of given size.
void *create_sm(size_t size)
{
  int prot = PROT_READ|PROT_WRITE;
  int visible = MAP_ANONYMOUS|MAP_SHARED;
  return mmap(NULL, size, prot, visible, -1, 0);
}

// It writes a message to local syslog from shared memory.
void sigusr1()
{
  syslog(LOG_INFO, "%s", ptr);
}

