
enum ptype {INIT=1, ACK=2, DATA=3, CLS=4};

struct data_pkt
{
  char type;
  char data[512];
  int id;
  char src[100];
  char dst[100];
};

struct ack_pkt
{
  char type;
  int id;
  char src[100];
  char dst[100];
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

