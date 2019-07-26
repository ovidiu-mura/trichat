#include "libs.h"


char * ser_data(void *pkt, char tp)
{
  char *ser;
  int off=0;
  if(tp == INIT)
  {
    ser = malloc(sizeof(char)+sizeof(int)+sizeof(char)*200);
    memcpy(ser, &((struct init_pkt*)pkt)->type, sizeof(char));
    off = sizeof(char);
    memcpy(ser+off+1, &((struct init_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off+1, &((struct init_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off+1, &((struct init_pkt*)pkt)->dst, sizeof(char)*100);
  }
  return ser;
}

char * deser_data(void *pkt)
{
  char *deser;
  char *tmp = (char*)pkt;
  if(tmp[0]==INIT)
  {
    deser = malloc(sizeof(struct init_pkt));
    memcpy(&((struct init_pkt*)deser)->type, &((struct init_pkt*)pkt)->type, sizeof(char));
    memcpy(&((struct init_pkt*)deser)->id, &((struct init_pkt*)pkt)->id, sizeof(int));
    memcpy(&((struct init_pkt*)deser)->src, &((struct init_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct init_pkt*)deser)->dst, &((struct init_pkt*)pkt)->dst, sizeof(char)*100);
  }
  return deser;
}
