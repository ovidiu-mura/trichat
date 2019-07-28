
enum ptype {INIT=1, ACK=2, DATA=3, CLS=4};

struct data_pkt
{
  char type;
  char data[467];
  int id;
  char src[20];
  char dst[20];
};

struct ack_pkt
{
  char type;
  int id;
  char src[20];
  char dst[20];
};

struct init_pkt
{
  char type;
  int id;
  char src[100];
  char dst[100];
};

struct cls_pkt
{
  char type;
  int id;
  char src[100];
  char dst[100];
};


char * ser_data(void *, char);
char * deser_data(void *);
struct init_pkt* deser_init_pkt(char*);
struct data_pkt* deser_data_pkt(char*);


char* hide_zeros(unsigned char*);
char* unhide_zeros(unsigned char*);
