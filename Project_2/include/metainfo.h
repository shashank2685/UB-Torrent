#ifndef _METAINFO_H_
#define _METAINFO_H_

int  populate_peices(uint8_t *);
int populate_tracker_struct(be_node * , int);
int populate_struct(be_node *n, char *buffer);
uint8_t *calculate_info_hash(char *buffer, int len);
int read_torrent_file(char *file_name);

#endif
