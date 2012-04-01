

#ifndef __DOWNLOAD_H__
#define __DOWNLOAD_H__


#define CHOKE           0
#define UNCHOKE         1
#define INTERSTED       2
#define NOT_INTERSTED   3
#define HAVE            4
#define BIT_FIELD       5
#define REQUEST         6
#define PEICE           7
#define CANCLE          8

#define NOT_DOWNLOADED      0
#define DOWNLOADING         1
#define COMPLETED           2

uint8_t *calculate_info_hash(char *, int);
int get_bit_field(uint8_t *, int);
void download_from_peers();
void set_bit_field(uint8_t *, int);
int establish_tcp_connection();
int read_from_client_connection(int index);
#endif
