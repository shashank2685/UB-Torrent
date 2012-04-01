#include "bencode.h"
#include "common_headers.h"
#include "ubtorrent.h"
#include "sha1.h"
#include "error_handlers.h"
#include "network_util.h"

extern struct global_info node;
extern struct tracker_info global_tracker_info;

int inside_info;
int bytes_written_to_buffer;

int populate_peices(uint8_t *peices) {

        int i = 0;
        node.peice_hashes = malloc(node.no_of_peices * sizeof(void *));

        for (i = 0; i < node.no_of_peices; i++) {
                node.peice_hashes[i] = (uint8_t *)malloc(20);
                memset(node.peice_hashes[i], 0, 20);
                memcpy(node.peice_hashes[i], peices + (i * 20), 20);
        }

}

int populate_tracker_struct(be_node *n , int sock_fd) {

        size_t i;
        int j;
        uint16_t local_port = ntohs(node.tcp_port);
        struct sockaddr_in local_address;
        int local_addr_len = sizeof(local_address);
        struct sockaddr_in temp_address;

        switch (n->type) {

                case BE_LIST:
                        for (i = 0; n->val.l[i]; ++i)
                                populate_tracker_struct(n->val.l[i], sock_fd);
                        break;

                case BE_DICT:
                        for (i = 0; n->val.d[i].val; ++i) {
                                //TODO To read the peer_id from the response and update if its available
                                //TODO if peer id available use it while construction request to tracker
                                if (strcmp(n->val.d[i].key, "interval") == 0)
                                        global_tracker_info.interval = n->val.d[i].val->val.i;
                                if (strcmp(n->val.d[i].key, "min interval") == 0)
                                        global_tracker_info.min_interval = n->val.d[i].val->val.i;
                                if (strcmp(n->val.d[i].key, "complete") == 0)
                                        global_tracker_info.complete = n->val.d[i].val->val.i;
                                if (strcmp(n->val.d[i].key, "downloaded") == 0)
                                        global_tracker_info.tracker_response_downloaded = n->val.d[i].val->val.i;
                                if (strcmp(n->val.d[i].key, "incomplete") == 0) 
                                        global_tracker_info.incomplete = n->val.d[i].val->val.i;
                                if (strcmp(n->val.d[i].key, "peers") == 0) {
                                        global_tracker_info.no_of_peers = ((be_str_len(n->val.d[i].val)) / 6) - 1;
                                        if (global_tracker_info.no_of_peers != 0) {
                                                if (getsockname(sock_fd, (struct sockaddr *)&local_address, &local_addr_len) < 0) {
                                                        report_error("getsockname()");
                                                        return 0;
                                                }
                                                if (global_tracker_info.peers != NULL) {
                                                        free(global_tracker_info.peers);
                                                        global_tracker_info.peers = NULL;
                                                }
                                                global_tracker_info.peers = malloc(sizeof(struct sockaddr_in) * global_tracker_info.no_of_peers);
                                                if (global_tracker_info.peers == NULL) {
                                                    report_error("malloc():");
                                                    return -1;
                                                }
                                                int iterator = 0;
                                                for (j = 0; j < global_tracker_info.no_of_peers + 1; j++) {
                                                    memset(&temp_address, 0, sizeof(temp_address));
                                                    memcpy(&temp_address.sin_addr.s_addr, n->val.d[i].val->val.s + (j * 6), 4);
                                                    memcpy(&temp_address.sin_port, (n->val.d[i].val->val.s + (j * 6)) + 4, 2);
                                                    if (local_port != temp_address.sin_port) {
                                                        global_tracker_info.peers[iterator].sin_addr.s_addr = temp_address.sin_addr.s_addr;
                                                        global_tracker_info.peers[iterator++].sin_port = temp_address.sin_port;
                                                    } else if (local_address.sin_addr.s_addr != temp_address.sin_addr.s_addr) {
                                                        global_tracker_info.peers[iterator].sin_addr.s_addr = temp_address.sin_addr.s_addr;
                                                        global_tracker_info.peers[iterator++].sin_port = temp_address.sin_port;
                                                    }
                                                }
                                        }
                                }
                                populate_tracker_struct(n->val.d[i].val, sock_fd);
                        }
                        break;
        }
}


int populate_struct(be_node *n, char *buffer) {

        size_t i;
        uint8_t *peices;
        int string_len = 0;

        switch (n->type) {
                case BE_STR:
                        if (inside_info) {
                                bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"%lli:", be_str_len(n));
                                memcpy(buffer + bytes_written_to_buffer, n->val.s, be_str_len(n));
                                bytes_written_to_buffer += be_str_len(n);
                        }
                        break;

                case BE_INT:
                        if (inside_info)
                                bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"i%llie", n->val.i);
                        break;

                case BE_LIST:

                        for (i = 0; n->val.l[i]; ++i) {
                                if (inside_info)
                                        bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"l");
                                populate_struct(n->val.l[i], buffer);
                                if (inside_info)
                                        bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"e");
                        }
                        break;

                case BE_DICT:
                        if (inside_info) 
                                bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"d");
                        for (i = 0; n->val.d[i].val; ++i) {
                                if (inside_info)
                                        bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"%d:%s", strlen(n->val.d[i].key), n->val.d[i].key);
                                if (strcmp(n->val.d[i].key, "announce") == 0) {
                                        string_len = be_str_len(n->val.d[i].val);
                                        node.announce_url = malloc(string_len + 1);
                                        if (node.announce_url == NULL) {
                                                sys_error("malloc()");
                                        }
                                        memset(node.announce_url, 0, string_len + 1);
                                        strcpy(node.announce_url, n->val.d[i].val->val.s);
                                        node.announce_url[string_len] = '\0';
                                }
                                if (strcmp(n->val.d[i].key, "name") == 0) {
                                        string_len = be_str_len(n->val.d[i].val);
                                        node.file_name = malloc(string_len + 1);
                                        if (node.announce_url == NULL) {
                                                sys_error("malloc()");
                                        }
                                        memset(node.file_name, 0, string_len + 1);
                                        strcpy(node.file_name, n->val.d[i].val->val.s);
                                        node.file_name[string_len] = '\0';
                                }
                                if (strcmp(n->val.d[i].key, "info") == 0) {
                                        /* calculate the info hash */
                                        inside_info = 1;
                                }
                                if (strcmp(n->val.d[i].key, "length") == 0) {
                                        node.file_size = n->val.d[i].val->val.i;
                                }
                                if (strcmp(n->val.d[i].key, "piece length") == 0) {
                                        node.peice_size = n->val.d[i].val->val.i;
                                }
                                if (strcmp(n->val.d[i].key, "pieces") == 0) {
                                        node.no_of_peices = (node.file_size % node.peice_size)?(node.file_size/node.peice_size) + 1:(node.file_size/node.peice_size);
                                        peices = malloc(node.no_of_peices * 20);
                                        if (peices == NULL) {
                                                sys_error("malloc():");
                                        }
                                        memset(peices, 0, 20 * node.no_of_peices);
                                        memcpy(peices, n->val.d[i].val->val.s, node.no_of_peices * 20);
                                        populate_peices(peices);
                                        free(peices);
                                }
                                populate_struct(n->val.d[i].val, buffer);
                                if (strcmp(n->val.d[i].key, "info") == 0) {
                                        /* calculate the info hash */
                                        inside_info = 0;
                                }
                        }
                        if (inside_info)
                                bytes_written_to_buffer += sprintf(buffer + bytes_written_to_buffer,"e");
                        break;
        }

}

uint8_t *calculate_info_hash(char *buffer, int len) {

        int         err;
        SHA1Context sha;
        uint8_t *message_digest = malloc(20); /* 160-bit SHA1 hash value */

        memset(message_digest, 0, 20);

        err = SHA1Reset(&sha);
        if (err) app_error("SHA1Reset error %d\n", err);

        /* 'seed' is the string we want to compute the hash of */
        err = SHA1Input(&sha, (const unsigned char *) buffer, len);

        if (err) app_error("SHA1Input Error %d.\n", err );

        err = SHA1Result(&sha, message_digest);
        if (err) {
                app_error("SHA1Result Error %d, could not compute message digest.\n", err);
        }
        return message_digest;
}

int read_torrent_file(char *file_name) {

        char buffer[8 * 1024];
        int fp, fp1;
        struct stat st;
        int bytes_read;
        be_node *n;
        uint8_t *message_digest;

        stat(file_name, &st);
        if (st.st_size > 8 * 1024) {
                report_error("I am complaining (to the screen)");
                report_error(".torrent file size should be less than 8KB");
                report_error("I cannot work with large .torrent files");
                exit(0);
        }

        fp = open(file_name, O_RDONLY);
        if (fp < 0) {
                app_error("Unable to open the .torrent file %s", file_name);
        }
        memset(buffer, 0, 8 * 1024);
        if ((bytes_read = readn(fp, buffer, st.st_size)) < 0) {
                sys_error("Unable to read the torrent file: %s", file_name);
        }
        close(fp);
        n = be_decoden(buffer, bytes_read);
        memset(buffer, 0, 8 * 1024);
        populate_struct(n, buffer);
        message_digest=calculate_info_hash(buffer, bytes_written_to_buffer);
        memcpy(node.info_hash, message_digest, 20);
        be_free(n);
}
