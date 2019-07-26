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
  else if(tp == ACK)
  {
    ser = malloc(sizeof(struct ack_pkt));
    memcpy(ser, &((struct ack_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off+1, &((struct ack_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off+1, &((struct ack_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off+1, &((struct ack_pkt*)pkt)->dst, sizeof(char)*100);
  }
  else if(tp == DATA)
  {
    ser = malloc(sizeof(struct data_pkt));
    memcpy(ser, &((struct data_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off+1, &((struct data_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off+1, &((struct data_pkt*)pkt)->data, sizeof(char)*512);
    off += sizeof(char)*512;
    memcpy(ser+off+1, &((struct data_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off+1, &((struct data_pkt*)pkt)->dst, sizeof(char)*100);
  }
  else if(tp == CLS)
  {
    ser = malloc(sizeof(struct cls_pkt));
    memcpy(ser, &((struct cls_pkt*)pkt)->type, sizeof(char));
    off += sizeof(char);
    memcpy(ser+off+1, &((struct cls_pkt*)pkt)->id, sizeof(int));
    off += sizeof(int);
    memcpy(ser+off+1, &((struct cls_pkt*)pkt)->src, sizeof(char)*100);
    off += sizeof(char)*100;
    memcpy(ser+off+1, &((struct cls_pkt*)pkt)->dst, sizeof(char)*100);
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
    memcpy(&((struct data_pkt*)deser)->src, &((struct data_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct data_pkt*)deser)->dst, &((struct data_pkt*)pkt)->dst, sizeof(char)*100);
    memcpy(&((struct data_pkt*)deser)->data, &((struct data_pkt*)pkt)->data, sizeof(char)*512);
  }
  else if(tmp[0] == CLS)
  {
    deser = malloc(sizeof(struct cls_pkt));
    memcpy(&((struct cls_pkt*)deser)->type, &((struct cls_pkt*)pkt)->type, sizeof(char));
    memcpy(&((struct cls_pkt*)deser)->id, &((struct cls_pkt*)pkt)->id, sizeof(int));
    memcpy(&((struct cls_pkt*)deser)->src, &((struct cls_pkt*)pkt)->src, sizeof(char)*100);
    memcpy(&((struct cls_pkt*)deser)->dst, &((struct cls_pkt*)pkt)->dst, sizeof(char)*100);
  }
  return deser;
}
