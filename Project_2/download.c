
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#include "error_handlers.h"
#include "common_headers.h"
#include "ubtorrent.h"
#include "bencode.h"
#include "download.h"
#include "network_util.h"

extern struct global_info node;
extern struct peer_status conn[];
extern struct tracker_info global_tracker_info ;
extern int sleep_time;

int get_free_sock(){
        int i;

        for (i = 0; i < MAX_CONN; i++) {
                if (!conn[i].in_use)
                        return i;
        }
        return -1;
}
void send_intersted(int index, int flag) {

        ENTER_FUNCTION

        char message[5];
        int len = htonl(1);
        uint8_t id;

        memset(message, 0, 5);
        memcpy(message, &len, 4);
        if (flag) {
                id = (uint8_t)2;
        } else {
                id = (uint8_t)3;
        }
        memcpy(message + 4, &id, 1);
        if (send(conn[index].sock_fd, message, 5, 0) < 0) {
                if (flag) {
                        report_error("Unable to send the intersted message\n");
                } else {
                        report_error("Unable to send the not intersted message\n");
                }
        }
//        printf("Sent intersted message to cid %d\n", index);
        LEAVE_FUNCTION
}
void find_rarest_peice_and_send_request() {

        ENTER_FUNCTION
                int minimum_ranking;
        int i = 0;
        int j = 0;
        int *same_ranking_peices;
        int rarest_peice_index;

        same_ranking_peices  = malloc(sizeof(int) * node.no_of_peices);
        memset(same_ranking_peices, 0, sizeof(int) * node.no_of_peices);
        // get smallest value
        minimum_ranking = node.peice_ranking[0];
        for (i = 0; i < node.no_of_peices; i++)
                minimum_ranking = minimum_ranking < node.peice_ranking[i] ? minimum_ranking: node.peice_ranking[i];
        if (minimum_ranking == 120) {
                // all the peices have been downloaded so dont 
                // do any thing stupid. Simply return from the 
                // function.
                return; 
        }
        // Get peices with same rankings
        for (i = 0; i < node.no_of_peices; i++) {
                if(node.peice_ranking[i] == minimum_ranking)
                        same_ranking_peices[j++] = i;
        }

        rarest_peice_index = same_ranking_peices[(rand() % j)];
        free(same_ranking_peices);
        // check if there is any peer who has the peice and have unchocked me
        // if yes send request for download to him if any request hasent sent to him
        // if the request has been sent to him select a new peer who has the peice 
        // and send him intersted message.
        for (i = 0; i < MAX_CONN; i++) {
                if (conn[i].in_use) {
                        // check if the peer has the peice
                        if (conn[i].bit_field != NULL) {
                                if (get_bit_field(conn[i].bit_field, rarest_peice_index) && node.peice_status[rarest_peice_index] == NOT_DOWNLOADED) {
                                        if (!conn[i].peer_chocking) {
                                                // if the peer is not chocking and is not busy
                                                // with some request send him the request
                                                if (!conn[i].request_sent) {
                                                        if (!conn[i].am_intersted) {
                                                                send_intersted(i, 1);
                                                                conn[i].am_intersted = 1;
                                                        }
//                                                        printf("Sent request for the peice %d ", rarest_peice_index);
//                                                        printf("To the connection %d\n", i);
                                                        conn[i].intersted_peice = rarest_peice_index;
                                                        node.peice_status[rarest_peice_index] = DOWNLOADING;
                                                        node.peice_ranking[rarest_peice_index] = 9999;
                                                        send_request(rarest_peice_index, i);
                                                        rarest_peice_index = -1;
                                                        conn[i].request_sent = 1;
                                                }
                                        } else {
                                                if (!conn[i].am_intersted) {
                                                        send_intersted(i, 1);
                                                        conn[i].am_intersted = 1;
                                                }
                                        }
                                }
                        }
                }
        }
        LEAVE_FUNCTION
}

int establish_conn_with_client(int index) {

        ENTER_FUNCTION
                uint8_t pstrlen = 19;
        char pstr[] = "BitTorrent protocol";
        uint8_t reserved[8];
        char handshake[68];

        int id = get_free_sock();

        global_tracker_info.peers[index].sin_family = AF_INET;
        conn[id].sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (conn[id].sock_fd < 0) {
                report_error("socket():");
                return -1;
        }

        if (connect(conn[id].sock_fd, (struct sockaddr *)&global_tracker_info.peers[index], sizeof(struct sockaddr_in)) < 0) {
                report_error("connect()");
                return -1;
        }
        conn[id].in_use = 1;
        conn[id].am_chocking = 1;
        conn[id].am_intersted = 0;
        conn[id].peer_chocking = 1;
        conn[id].peer_intersted = 0;
        time(&conn[index].down_start_time);
        // Send hand shake message to the client
        memset(handshake, 0, 68);
        memset(reserved, 0, 8);
        memcpy(handshake, &pstrlen, 1);
        memcpy(handshake + 1, pstr, 19);
        memcpy(handshake + 20, reserved, 8);
        memcpy(handshake + 28, node.info_hash, 20);
        memcpy(handshake + 48, node.peer_id, 20);

        if (send(conn[id].sock_fd, handshake, 68, 0) < 0) {
                report_error("send(): handshake message");
                return 0;
        }

        //    printf("Sent Handshake message \n");
        // Wait for the handshake message form the  peer
        memset(handshake, 0, 68);
        readn(conn[id].sock_fd, handshake, 68);
        if (memcmp(&node.info_hash, handshake + 28, 20) != 0) {
                close(conn[id].sock_fd);
                conn[id].in_use = 0;
        }
        //    printf("Recevived Valid Handshake message \n");

        // Sending Bit field of the client
        memset(handshake, 0, 68);
        int bit_field_size = (node.no_of_peices % 8) ? node.no_of_peices / 8 + 1: node.no_of_peices / 8;
        int temp;
        uint8_t message_id = 5;
        bit_field_size++;

        temp = htonl(bit_field_size);
        memcpy(handshake, &temp, sizeof(bit_field_size));
        memcpy(handshake + sizeof(bit_field_size), &message_id, sizeof(message_id));
        memcpy(handshake + 5, node.bit_field, bit_field_size - 1);
        if (send(conn[id].sock_fd, handshake, 4 + bit_field_size, 0) < 0) {
                report_error("send(): bitfield message");
                return 0;
        }

        //    printf("Sent the bit_field message \n");
        LEAVE_FUNCTION
}


void download_from_peers() {

        ENTER_FUNCTION
                int i, ret;
        for (i = 0; i < global_tracker_info.no_of_peers; i++) {
                //Establish connection with the client and add the
                //sock fd to the conn list.
                establish_conn_with_client(i);
        }
        LEAVE_FUNCTION
}

int get_message_id(char *message, uint8_t *id, int *length) {

        int temp;
        memcpy(&temp, message, 4);
        *length = ntohl(temp);
        memcpy(id, message + 4, 1);
}

uint8_t *get_bit_field_message(int index, int length) {

        ENTER_FUNCTION
                uint8_t *bit_field =  malloc (sizeof(uint8_t) * (length -1));
        int bit_field_size = (node.no_of_peices % 8) ? node.no_of_peices / 8 + 1: node.no_of_peices / 8 ;
        int nbytes = 0, i;

        if (bit_field == NULL) {
                report_error("malloc()");
                return NULL;
        }
        memset(bit_field, 0, length - 1);
        if ((nbytes = readn(conn[index].sock_fd, bit_field, length - 1)) < 0) {
                report_error("Error: while reading form socket");
                return NULL;
        }
        if ((length - 1) != bit_field_size) {
                warning("Error: Mismatch in bit_field_size");
                return NULL;
        }
        //if spare bits in bit_field are set return NULL
        for (i = node.no_of_peices; i % 8; i++) {
                if (get_bit_field(bit_field, i)) {
                        printf("Error: Bit field has the spare bits set\n");
                        return NULL;
                }
        }
        for (i = 0; i < node.no_of_peices; i++) {
                if (get_bit_field(bit_field, i))
                        node.peice_ranking[i]++;
        }
        LEAVE_FUNCTION
                return bit_field;
}

int get_bit_field(uint8_t *bit_field, int position) {

        int i;
        int j = 0;
        uint8_t shift = 128;

        for (i = 0; i < position; i++) {
                shift = shift >> 1;
                if (i != 0 && (i % 8) == 0) {
                        j++;
                        shift = 128;
                }
        }
        return (bit_field[j] & shift);
}

int check_intersted(int index) {
        ENTER_FUNCTION
                int i;
        for (i = 0; i < node.no_of_peices; i++) {
                if (!get_bit_field(node.bit_field, i)) {
                        // I dont have this peice
                        // check if the peice is there witih the peer
                        if(get_bit_field(conn[index].bit_field, i)) 
                                if (node.peice_status[i] == NOT_DOWNLOADED) {
                                        LEAVE_FUNCTION
                                                return i;
                                }
                }
        }
        LEAVE_FUNCTION
                return -1;
}


int send_request(int intersted, int conn_index) {
        ENTER_FUNCTION
                int msg_length = htonl(13);
        uint8_t id = REQUEST;
        uint32_t index = htonl(intersted);
        uint32_t begin = htonl(0);
        uint32_t length;
        char message[17];
        int temp;

        // check if the peice is the last peice
        // if its an last peice request for the approiate bytes
        if (intersted == node.no_of_peices - 1) {
                //last peice. so request only the relevnet bytes
                temp = node.file_size % node.peice_size;
                if (temp) {
                        length = htonl(temp);
                } else {
                        length = htonl(node.peice_size);
                }
        } else {
                length = htonl(node.peice_size);
        }
        memset(message, 0, 17);
        memcpy(message, &msg_length, 4);
        memcpy(message + 4, &id, 1);
        memcpy(message + 5, &index, 4);
        memcpy(message + 9, &begin, 4);
        memcpy(message + 13, &length, 4);
        if (send(conn[conn_index].sock_fd, message, 17, 0) < 0) {
                report_error("Unable to send request message");
                return 0;
        }
        //    printf("Sent request message for index %d to the peer at conn id %d\n", intersted, conn_index);
        LEAVE_FUNCTION
                return 0;
}

void set_bit_field(uint8_t *bit_field, int bit_position) {

        int i;
        uint8_t shift = 128;
        int j = 0;

        for (i = 0; i < bit_position; i++) {

                shift = shift >> 1;
                if (i != 0 && (i % 8) == 0) {
                        j++;
                        shift = 128;
                }
        }
        bit_field[j] = bit_field[j] | shift;
}

int send_have_message(int peice_index) {

        int i;
        uint32_t length = htonl(5);
        uint8_t message_id = htonl(4);
        uint32_t msg_peice_index = htonl(peice_index);
        char message[9];
        // Construct have message for the index

        memcpy(message, &length, 4);
        memcpy(message + 4, &message_id, 1);
        memcpy(message + 5, &msg_peice_index, 4);

        for (i = 0; i < MAX_CONN; i++) {
                if (conn[i].in_use) {
                        if (send(conn[i].sock_fd, message, 9, 0) < 0) {
                                report_error("Unable to send have message to peer wiht CID %d\n", i);
                        }
                }
        }    
}

int close_connection_to_seeders() {

        int i, j;

        for (i = 0; i < MAX_CONN; i++) {
                if (conn[i].in_use) {
                        //Check if its a seeder
                        for (j = 0; j < node.no_of_peices; j++) {
                                if (!get_bit_field(conn[i].bit_field, j))
                                        break;
                        }
                        if (j == node.no_of_peices) {
                                // i is a seeder. close the connection
                                close(conn[i].sock_fd);
                                conn[i].in_use = 0;
                        }
                }
        }
}

int read_peice(int index, int length) {

        ENTER_FUNCTION
                uint32_t temp;
        uint32_t peice_index;
        uint32_t offset;
        uint32_t peice_length;
        uint8_t *peice_digest;
        int bytes_read = 0;
        char *peice;
        int intersted, i;

        if (readn(conn[index].sock_fd, &temp, 4) != 4) {
                report_error("Unable to read index from message");
                close(conn[index].sock_fd);
                conn[index].in_use = 0;
                return 0;
        }

        peice_index = htonl(temp);
        if (readn(conn[index].sock_fd, &temp, 4) != 4) {
                report_error("Unable to read offset from message");
                close(conn[index].sock_fd);
                conn[index].in_use = 0;
                return 0;
        }

        if (conn[index].intersted_peice != peice_index) {
                report_error("Peice sent by the peer doesnt  match with what was requested");
                close(conn[index].sock_fd);
                conn[index].in_use = 0;
                return 0;
        }
        offset = htonl(temp);
        peice_length = length - 9;
        peice = malloc(sizeof(uint8_t) * peice_length);
        if (peice == NULL) {
                report_error("alloc");
                close(conn[index].sock_fd);
                conn[index].in_use = 0;
                return 0;
        }
        readn(conn[index].sock_fd, peice, peice_length);
        time(&conn[index].down_end_time);
        if (conn[index].down_end_time == conn[index].down_start_time) {
                conn[index].downspeed = peice_length;
        } else {
                conn[index].downspeed = peice_length / (int) (conn[index].down_end_time - conn[index].down_start_time);
        }
        //verify the hash
        peice_digest = calculate_info_hash(peice, peice_length);
        if (memcmp(peice_digest, node.peice_hashes[peice_index], 20) != 0) {
                report_error("mismatch with the sent peice\n");
                close(conn[index].sock_fd);
                conn[index].in_use = 0;
                return 0;
        }
        conn[index].request_sent = 0;
        (void)fseek(node.fp, 0L, SEEK_SET);
        fseek(node.fp, peice_index * node.peice_size, SEEK_SET);
        if (fwrite(peice, 1, peice_length, node.fp) != peice_length) {
                report_error("Error while writing to the  file. Remove the file and restart the ubtorrent again");
                exit(0);
        }
        free(peice);
        //    printf("Got the peice %d from the peer at conn id %d\n", peice_index, index);
        global_tracker_info.downloaded += peice_length;
        global_tracker_info.left -= peice_length;
        node.peice_status[peice_index] = COMPLETED;

        //check weather all the peices are downloaded completely
        for (i = 0; i < node.no_of_peices; i++) {
                if (node.peice_status[i] != COMPLETED)
                        break;
        }
        set_bit_field(node.bit_field, peice_index);
        // inform all other peer about your downloaded peice
        send_have_message(peice_index);

        if ( i == node.no_of_peices) {
                printf("\n\t **********  Congratuations! Download Completed ***********\n");
                // close all the connections to the seeder.
                // Connections to the leeacher should be kept alive
                // close_connection_to_seeders();
                // send not intersted message
                send_intersted(index, 0);
                announce(2);
                return 0;
        }

        if ((intersted = check_intersted(index)) >= 0) {
                conn[index].am_intersted = 1;
                //        conn[index].intersted_peice = intersted;
                //        send_intersted(index, 1);
                //        node.peice_status[intersted] = DOWNLOADING;
                //        send_request(conn[index].intersted_peice, index);
//                printf("Calling rarest peice algo\n");
                find_rarest_peice_and_send_request();
        } else {
                conn[index].am_intersted = 0;
                send_intersted(index, 0);
        }
        LEAVE_FUNCTION
}

int read_have_message(int index, int length) {

        int temp = 0;
        int peice_index = 0;
        readn(conn[index].sock_fd, &temp, 4);
        peice_index = ntohl(temp);
        //    printf("Peer sent have message for the peer %d\n", peice_index);
        set_bit_field(conn[index].bit_field, peice_index);
        node.peice_ranking[peice_index]++;
        // check weather you are intersted in the peice
        if (!get_bit_field(node.bit_field, peice_index)) {
                // If the peice is in not downloaded state
                // send the intersted message and send in 
                // request for that peice
                if (node.peice_status[peice_index] == NOT_DOWNLOADED) {
                        send_intersted(index, 1);
                        //                send_request(peice_index, index);
                        find_rarest_peice_and_send_request();
                }
        }
}

int establish_tcp_connection() {

        int index = get_free_sock();
        struct sockaddr_in client_address;
        int addr_len = sizeof(struct sockaddr_in);
        char handshake[68];
        uint8_t pstrlen;
        char pstr[19];
        uint8_t info_hash[20];

        if (index < 0) {
                warning("\nRequest for a connection from a peer is received, But\n");  
                warning("Maximum number of allowed connections are reached\n");
                return 0;
        }
        memset(&client_address, 0, addr_len);
        conn[index].sock_fd = accept(node.tcp_sockfd,
                        (struct sockaddr *) &(client_address),
                        &addr_len);
        conn[index].in_use = 1;
        conn[index].am_chocking = 1;
        conn[index].am_intersted = 0;
        conn[index].peer_chocking = 1;
        conn[index].peer_intersted = 0;
        time(&conn[index].up_start_time);
        //    printf("\nReceived the connection form the peer\n");
        //Read hand shake message
        memset(handshake, 0, 68);
        if (readn(conn[index].sock_fd, handshake, 68) < 0) {
                printf("Unable to read handshake message form peer\n");
                return 0;
        }
        memcpy(&pstrlen, handshake, 1);
        if (pstrlen != 19) {
                printf("\n\t This client only supports BITTorrent protocol\n");
                conn[index].in_use = 0;
                close(conn[index].sock_fd);
                return 0;
        }

        memcpy(pstr, handshake + 1,pstrlen);
        if (strncmp(pstr, "BitTorrent protocol", 19) != 0) {
                printf("\n\t This client only supports BITTorrent protocol\n");
                conn[index].in_use = 0;
                close(conn[index].sock_fd);
                return 0;
        }
        memcpy(info_hash, handshake + 28, 20);
        if (memcmp(info_hash, node.info_hash, HASH_SIZE) != 0) {
                printf("\n\t Mismatch between the info_hash of client and the\n");
                printf("\t one sent by peer. closing the connection \n");
                conn[index].in_use = 0;
                close(conn[index].sock_fd);
                return 0;
        }
        //    printf("Read handshake message successfully\n");
        // Everything went well. send the handshake message in reply.
        pstrlen = 19;
        strncpy(pstr, "BitTorrent protocol", 19);
        memset(handshake, 0, 68);
        memcpy(handshake, &pstrlen, 1);
        memcpy(handshake + 1, pstr, 19);
        memset(handshake + 20, 0, 8);
        memcpy(handshake + 28, node.info_hash, 20);
        memcpy(handshake + 48, node.peer_id, 20);
        if (send(conn[index].sock_fd, handshake, 68, 0) < 0) {
                report_error("send(): handshake message");
                conn[index].in_use = 0;
                close(conn[index].sock_fd);
                return 0;
        }
        //    printf("Sent handshake reply successfully\n");
        // Sending Bit field of the client
        memset(handshake, 0, 68);
        int bit_field_size = (node.no_of_peices % 8) ? node.no_of_peices / 8 + 1: node.no_of_peices / 8;
        int temp;
        uint8_t message_id = 5;
        bit_field_size++;

        temp = htonl(bit_field_size);
        memcpy(handshake, &temp, sizeof(bit_field_size));
        memcpy(handshake + sizeof(bit_field_size), &message_id, sizeof(message_id));
        memcpy(handshake + 5, node.bit_field, bit_field_size - 1);
        if (send(conn[index].sock_fd, handshake, 4 + bit_field_size, 0) < 0) {
                report_error("send(): bitfield message");
                return 0;
        }
        conn[index].bit_field = NULL;
        //    printf("Sent my bit field to the peer\n");
}

int read_request_message(int index, int length) {

        uint32_t temp;
        uint32_t peice_index;
        uint32_t begin;
        uint32_t peice_length;
        char *message;
        char *send_message;
        uint8_t message_id = 7;
        time_t now;

        if (conn[index].peer_intersted && !(conn[index].am_chocking)) {
                readn(conn[index].sock_fd, &temp, 4);
                peice_index = ntohl(temp);
                readn(conn[index].sock_fd, &temp, 4);
                begin = ntohl(temp);
                readn(conn[index].sock_fd, &temp, 4);
                peice_length = ntohl(temp);
                if (peice_index > node.no_of_peices - 1 || peice_index < 0)
                        return 0;
                if (begin + peice_length > node.peice_size) 
                        return 0;
                //        printf("received the request from the connectionn %d for the peice %d bigin = %d peice_length = %d\n", 
                //        index, peice_index, begin, peice_length);
                if (conn[index].bytes_uploaded != 0) {
                        time(&now);
                        if (now == conn[index].up_start_time) {
                                conn[index].upspeed = conn[index].bytes_uploaded;
                        } else {
                                conn[index].upspeed = conn[index].bytes_uploaded / (now - conn[index].up_start_time);
                        }
                }
                conn[index].bytes_uploaded = peice_length;
                message = malloc(peice_length);
                if (message == NULL) {
                        report_error("malloc()");
                        return 0;
                }
                if (get_bit_field(node.bit_field, peice_index)) {
                        (void)fseek(node.fp, 0L, SEEK_SET);
                        fseek(node.fp, (peice_index * node.peice_size) + begin, SEEK_SET);
                        if (fread(message, 1, peice_length, node.fp) != peice_length) {
                                warning("fread unable to read %d bytes from file\n", peice_length);
                                free(message);
                                return 0;
                        }
                        send_message = malloc(peice_length + 13);
                        if (send_message == NULL) {
                                report_error("alloc()");
                                free(message);
                                return 0;
                        }
                        temp = htonl(peice_length + 9);
                        memcpy(send_message, &temp, 4);
                        memcpy(send_message + 4, &message_id, 1);
                        temp = htonl(peice_index);
                        memcpy(send_message + 5, &temp, 4);
                        temp = htonl(begin);
                        memcpy(send_message + 9, &temp, 4);
                        memcpy(send_message + 13, message, peice_length);
                        sleep(sleep_time);
                        if (send(conn[index].sock_fd, send_message, peice_length + 13, 0) < 0) {
                                warning("Unable to send the peice message \n of index %d to the peer at CID %d \n", peice_index, index);
                                free(send_message);
                        }
                        global_tracker_info.uploaded += peice_length;
                        free(send_message);
                }
                free(message);
        }
}


int send_unchoke(int index) {

        uint32_t len;
        uint8_t id = 1;
        char message[5];

        memset(message, 0, 5);
        len = htonl(1);
        memcpy(message, &len, 4);
        memcpy(message + 4, &id, 1);

        if (send(conn[index].sock_fd, message, 5, 0) < 0) {
                printf("Unable to send unchoke messae to the connectio at cid %d", index);
        }
        //    printf("Sent unchoke message\n");
}

int read_from_client_connection(int index) {

        ENTER_FUNCTION
                char buffer[5];
        uint8_t message_id;
        int intersted;
        int length, nbytes;
        uint8_t *bit_field;
        uint32_t temp;

        memset(buffer, 0, 5);
        if ((nbytes = readn(conn[index].sock_fd, &temp, 4)) != 4) {
                if (nbytes == 0) {
                        // For some reason client closed the connection
                        printf("closed connection %d\n", index);
                        close(conn[index].sock_fd);
                        conn[index].in_use = 0;
                        return 0;
                }
                printf("Unable to read 4 bytes form the socket\n");
                return 0;
        }

        length = ntohl(temp);
        if (length == 0) {
                // Its keep alive message. For now just ignore the message
                return 0;
        } 
        if ((nbytes = readn(conn[index].sock_fd, &message_id, 1)) != 1) {
                if (nbytes == 0) {
                        // For some reason client closed the connection
                        close(conn[index].sock_fd);
                        conn[index].in_use = 0;
                        return 0;
                }
                printf("Unable to read 1 byte form the socket\n");
                return 0;
        }

        //    get_message_id(buffer, &message_id, &length);
        //    printf("message id = %d, length = %d\n", message_id, length);
        switch(message_id) {

                case CHOKE:
//                        printf("Got choke message\n");
                        conn[index].peer_chocking = 1;
                        break;
                case UNCHOKE:
//                        printf("Got un choke message\n");
                        conn[index].peer_chocking = 0;
                        if (conn[index].am_intersted) {
                                //node.peice_status[conn[index].intersted_peice] = DOWNLOADING;
                                find_rarest_peice_and_send_request();
                                // send_request(conn[index].intersted_peice, index);
                        }
                        break;
                case INTERSTED:
 //                       printf("Read intersted message at cid = %d\n", index);
                        conn[index].peer_intersted = 1;
                        conn[index].am_chocking = 0;  /* For Now just un choke the client */
                        /* once i implement optmistic unchoke algo */
                        /* (Not in near future) */
                        /* this line will be removed from here */
                        send_unchoke(index);
                        break;
                case NOT_INTERSTED:
  //                      printf("Read not intersted message at cid = %d\n", index);
                        conn[index].peer_intersted = 0;
                        break;
                case BIT_FIELD:
   //                     printf("Read bit message at cid = %d\n", index);
                        bit_field = get_bit_field_message(index, length);
                        if (bit_field == NULL) {
                                close(conn[index].sock_fd);
                                conn[index].in_use = 0;
                                break;
                        }
                        conn[index].bit_field = bit_field;
                        // If the peer is not chocking
                        // Send the request for the peice
                        if ((intersted = check_intersted(index)) >= 0) {
                                conn[index].am_intersted = 1;
                                conn[index].intersted_peice = intersted;
                                send_intersted(index, 1);
                                find_rarest_peice_and_send_request();
                                // node.peice_status[intersted] = DOWNLOADING;
                        } else {
                                conn[index].am_intersted = 0;
                                send_intersted(index, 0);
                        }
                        break;
                case HAVE:
    //                    printf("Read have message at cid = %d\n", index);
                        read_have_message(index, length);
                        break;
                case REQUEST:
     //                   printf("Read request at cid = %d\n", index);
                        read_request_message(index, length);
                        break;
                case PEICE:
      //                  printf("Read peice message at cid = %d\n", index);
                        read_peice(index, length);
                        break;
                case CANCLE:
                        // TODO implement the action for cancle message
                        // But now i dont know what action to take when this message
                        // is received, so ignore the message
                        break;
                default:
                        warning("Illegal message_id in the recived message\n");
                        // Some thing went wrong. To avoid disaster close the connection
                        close(conn[index].sock_fd);
                        conn[index].in_use = 0;
        }
        LEAVE_FUNCTION
}
