

#ifndef _UBTORRENT_H
#define _UBTORRENT_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#define ANNOUNCE        0
#define METAINFO        1
#define TRACKERINFO     2
#define SHOW            3
#define STATUS          4
#define QUIT            5

#define HASH_SIZE       20
#define PACKET_SIZE     2000
#define MAX_PORT_SIZE   10 
#define COMMAND_LIMIT   200

struct global_info {
    
    uint8_t info_hash[HASH_SIZE];
    uint8_t peer_id[HASH_SIZE];
    int tcp_sockfd;
    FILE *fp;
    uint16_t tcp_port;
    struct sockaddr_in tcp_address;
    char *metainfo_filename ;
    char *file_name; 
    unsigned long int file_size;
    unsigned long int peice_size;
    char *announce_url;
    int no_of_peices;
    uint8_t **peice_hashes;
    uint8_t *bit_field;
    int *peice_status;
    int *peice_ranking;
};

struct tracker_info {

    int complete;
    int downloaded;
    int tracker_response_downloaded;
    int uploaded;
    int left;
    int incomplete;
    int interval;
    int min_interval;
    char *tracker_id;
    int no_of_peers;
    struct sockaddr_in *peers;
};

struct peer_status {
    
    int     in_use;
    int     am_chocking;
    int     am_intersted;
    int     peer_chocking;
    int     request_sent;
    int     intersted_peice;
    int     peer_intersted;
    time_t  down_start_time;
    time_t  down_end_time;
    time_t  up_start_time;
    int     bytes_uploaded;
    int     downspeed;
    int     upspeed;
    int     sock_fd;
    uint8_t *bit_field;

};

#define MAX_CONN    10
#endif /* _UBTORRENT_H  */
