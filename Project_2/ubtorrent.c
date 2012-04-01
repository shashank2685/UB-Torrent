#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>


#include "error_handlers.h"
#include "common_headers.h"
#include "ubtorrent.h"
#include "bencode.h"
#include "download.h"
#include "metainfo.h"
#include "network_util.h"

void *launch_download_and_upload_threads(void *);
char *command_index[] = { 
        "announce",
        "metainfo",
        "trackerinfo",
        "show",
        "status",
        "quit"
};

int seeder_flag = 0;

struct global_info node;
struct tracker_info global_tracker_info;
struct peer_status conn[MAX_CONN];

int sleep_time;

void init_global_structs() {

        int i;
        for (i = 0; i < MAX_CONN; i++)
                conn[i].in_use = 0;
        node.metainfo_filename = NULL;
        node.file_name = NULL;
        node.announce_url = NULL;
        node.peice_hashes = NULL;
        node.bit_field    = NULL;
        global_tracker_info.tracker_id = NULL;
        global_tracker_info.peers = NULL;

}

int url_encode(char *dest, uint8_t *source) {

        int i;
        char perc = '%';
        int bytes_written = 0;

        for (i = 0; i < HASH_SIZE; i++) {
                if (isalnum(source[i]) || source[i] == '.' ||
                            source[i] == '-' || source[i] == '_' || 
                            source[i] == '~') {
                        sprintf(dest, "%c", source[i]);
                        bytes_written += 1;
                        dest += 1;
                } else {
                        sprintf(dest++, "%%");
                        sprintf(dest, "%02x", source[i]);
                        dest += 2;
                        bytes_written += 3;
                }
        }
        return bytes_written;
}

int create_packet_to_tracker(char *message, char *event) {

        int increment_pointer = 0;
        int bytes_written = 0;

        memset(message, 0, PACKET_SIZE);

        increment_pointer += strlen("GET /announce?info_hash=");
        sprintf(message, "GET /announce?info_hash=");

        /* Copy the info hash to the packet */
        bytes_written = url_encode(message + increment_pointer, node.info_hash);
        increment_pointer += bytes_written;

        increment_pointer += sprintf(message + increment_pointer, "&peer_id=");
        increment_pointer += url_encode(message + increment_pointer, node.peer_id);
        increment_pointer += sprintf(message + increment_pointer, 
                        "&port=%d&uploaded=%d&downloaded=%d&left=%d&compact=1&event=%s HTTP/1.1\r\n\n",
                        node.tcp_port, global_tracker_info.uploaded, global_tracker_info.downloaded, global_tracker_info.left,event);
        message[increment_pointer] = '\0';

#ifdef DBG
        printf("Sending Get request to tracker %s\n", message);
        exit(0);
#endif
        return increment_pointer;
}

void get_tracker_host_name_and_port(char *announce_url, char *tracker, char *port) {

        char *tracker_port;
        char *announce_copy;
        char *temp;

        announce_copy = strdup(announce_url);
        strtok(announce_copy, "/");
        tracker_port = strtok(NULL, "/");
        /* Now tracker_port has the both host name and port in it */
        /* Read only host name from the tracker_port */
        temp = strtok(tracker_port, ":");
        if (temp == NULL) {
                app_error("Unable to read tracker host name form the tracker url\n");
        }
        strncpy(tracker, temp, NI_MAXHOST);

        /* read port number from the string */
        temp = strtok(NULL, ":");
        strncpy(port, temp, MAX_PORT_SIZE);
        free(announce_copy);

}

int update_tracker_response(char *message, int content_length, int sock_fd) {

        be_node *n;
        n = be_decoden(message, content_length);
        if (populate_tracker_struct(n, sock_fd) < 0) {
                warning("Unable to update the tracker response\nTry again Untill it suceeds\n");
                warning("Tracker info command may give inconsistenet data\n");
        }
        be_free(n);
} 

int display_tracker_info() {

        int i;
        char ip[INET_ADDRSTRLEN];
        uint16_t port;

        if (global_tracker_info.peers == NULL && global_tracker_info.min_interval == 0) {
                printf("\n\n\tCant display any data without having it. Can i?\n");
                printf("\tExecute announce before executing this command \n\n");
                return 0; 
        }
        printf("completed | downloaded | incomplete | interval | min interval |\n");
        printf("---------------------------------------------------------------\n");
        printf("%9d | %10d | %10d | %8d | %12d |\n", global_tracker_info.complete, global_tracker_info.tracker_response_downloaded, global_tracker_info.incomplete,
                        global_tracker_info.interval, global_tracker_info.min_interval); 
        printf("---------------------------------------------------------------\n");
        if (global_tracker_info.no_of_peers > 0) {
                printf("+++ Peer List ( Self not included )\n");
                printf("IP                | Port\n");
                printf("---------------------------\n");
                for (i = 0; i < global_tracker_info.no_of_peers; i++) {
                        memset(ip, 0, INET_ADDRSTRLEN);
                        memset(&port, 0, sizeof(port));
                        if (inet_ntop(AF_INET, &global_tracker_info.peers[i].sin_addr, ip, INET_ADDRSTRLEN) == NULL) {
                                report_error("inet_ntop()");
                                continue;
                        }
                        port = ntohs(global_tracker_info.peers[i].sin_port);
                        printf("%-18s| %-6d\n", ip, port);
                }
        }
}

int read_from_tracker(char *message, int sock_fd)
{
        int bytes_read =0;
        long content_length = 0;
        char *temp;
        do {
                memset(message, 0, PACKET_SIZE);
                if ((bytes_read = readline(sock_fd, message, PACKET_SIZE))< 0) {
                        report_error("Unable to read from sock_fd");
                        return -1;
                }

                if (strstr(message, "HTTP") != NULL) {
                        /* read the status form the message */
                        printf("+++ Tracker Responded: ");
                        printf("%s\n", message);
                        temp = strtok(message, " ");
                        temp = strtok(NULL, " ");
                        if  (temp != NULL) {
                                if ((atoi(temp) % 100) > 2) {
                                        warning("Tracker returned error message with the status of %s", temp);
                                        return -1;
                                }
                        } else {
                                warning("Unable to read the status form the reply of the tracker");
                                return -1;
                        }
                        continue;
                }
                if (strstr(message, "Content-Length") != NULL) {
                        /* Read the content length */
                        strtok(message, " ");
                        temp = strtok(NULL, " ");
                        if (temp == NULL) {
                                warning("Unable to read the Content length form the reply of the tracker");
                                return -1;
                        }
                        content_length = strtol(temp, NULL, 10);
                        continue;
                }

                if (strcmp(message, "\r\n") == 0) {
                        /* read empty line */
                        /* Next line contains the data which of interest ot us */
                        memset(message, 0, PACKET_SIZE);
                        if ((bytes_read = readline(sock_fd, message, PACKET_SIZE))< 0) {
                                report_error("Unable to read from sock_fd");
                                return -1;
                        }
                        if (update_tracker_response(message, content_length, sock_fd) < 0) {
                                warning("Unable to store the traker response\n");
                                return -1;
                        }
                        display_tracker_info();
                }
                if (bytes_read == 0) {
                        /* Tracker closed the connection */
                        /* Nothing more to do. Return */
                        printf("Tracker closed the connection......\n");
                        close(sock_fd);
                        return 0;
                }
        } while(bytes_read != 0);
}

int announce(int start) {

        char *message;
        int message_length;
        struct addrinfo *tracker_address;
        struct addrinfo hint;
        char tracker[NI_MAXHOST + 1];
        char tracker_port[MAX_PORT_SIZE];
        int sock_fd;
        int bytes_sent = 0;
        

        /* Establish tcp connection with the tracker */
        /* and read the relevent iformation */
        memset(tracker, 0, NI_MAXHOST + 1);
        memset(tracker_port, 0, MAX_PORT_SIZE);
        get_tracker_host_name_and_port(node.announce_url, tracker, tracker_port);
        memset(&tracker_address, 0, sizeof(tracker_address));

        memset(&hint, 0, sizeof(hint));
        hint.ai_family = AF_INET;
        hint.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(tracker, tracker_port, &hint, &tracker_address) < 0) {
                report_error("getaddrinfo()");
                return -1;
        }
        sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
                report_error("socket()");
                return -1;
        }
        message = (char *)malloc(PACKET_SIZE);
        if (message == NULL) {
                report_error("malloc()");
                return -1;
        }
        if (start == 1) {
                if ((message_length = create_packet_to_tracker(message, "started")) < 0) {
                        /* Unable to create the packet to return formt he function */
                        warning("Unable to send the packet to tracker\n");
                        warning("Try again after some time\n");
                        return -1;
                } 
        } else if (start == 0){
                if ((message_length = create_packet_to_tracker(message, "stopped")) < 0) {
                        /* Unable to create the packet to return formt he function */
                        warning("Unable to send the packet to tracker\n");
                        warning("Try again after some time\n");
                        return -1;
                } 
        } else {
                if ((message_length = create_packet_to_tracker(message, "completed")) < 0) {
                        /* Unable to create the packet to return formt he function */
                        warning("Unable to send the packet to tracker\n");
                        warning("Try again after some time\n");
                        return -1;
                } 
        }
        if (connect(sock_fd, tracker_address->ai_addr, tracker_address->ai_addrlen) < 0) {
                report_error("conncet()");
                return -1;
        }

        if ((bytes_sent = send(sock_fd, message, message_length, 0)) < 0) {
                report_error("Unable to send message to tracker");
                free(message);
                freeaddrinfo(tracker_address);
                return -1;
        }
        if (start == 1) 
                read_from_tracker(message, sock_fd);
        free(message);
        freeaddrinfo(tracker_address);
}

int display_metainfo() {

        struct addrinfo hints;
        struct addrinfo *result;
        struct sockaddr_in *temp;
        void *in_addr;
        char ipaddress[INET_ADDRSTRLEN];
        int i = 0;
        int j = 0;

        memset(&hints, 0, sizeof(hints)); 
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo("localhost", NULL, &hints, &result) < 0) {
                report_error("getaddrinfo()");
                return -1;
        }

        temp = (struct sockaddr_in *) result->ai_addr;
        in_addr = &(temp->sin_addr);

        inet_ntop(AF_INET, in_addr, ipaddress, INET_ADDRSTRLEN);
        printf("\n  my IP/port \t: %s/%d\n", ipaddress, node.tcp_port);
        printf("  my ID \t: ");
        for (i = 0; i < 20; i++) {
                printf("%02x", node.peer_id[i]);
        }
        printf("\n");
        printf("  metainfo file : %s\n", node.metainfo_filename);
        printf("  info hash \t: ");
        for (i = 0; i < 20; i++) {
                printf("%02x", node.info_hash[i]);
        }
        printf("\n");
        printf("  file name \t: %s\n", node.file_name);
        printf("  piece length \t: %lu\n", node.peice_size);
        printf("  file size \t: %lu ", node.file_size);
        printf(" ( %lu * [peice_length ] + %lu)\n", 
                        (long unsigned int) node.no_of_peices - 1, 
                        (long unsigned int) node.file_size % node.peice_size);
        printf("  announce url \t: %s\n", node.announce_url);
        printf("  pieces hashes :\n");
        for ( i = 0; i < node.no_of_peices; i++) {
                printf("\t %d ", i);
                for (j = 0; j < 20; j++)
                        printf("%02x", node.peice_hashes[i][j]);
                printf("\n");
        }

        printf("\n\n");
        freeaddrinfo(result);
}

void status() {

        int i = 0;
        printf(" Downloaded | Uploaded |        Left | My Bit Field \n");
        printf("----------------------------------------------------\n");
        printf(" %10d | %8d | %11d | ",global_tracker_info.downloaded, global_tracker_info.uploaded, global_tracker_info.left);
        int bytes = node.no_of_peices % 8? node.no_of_peices / 8 + 1 : node.no_of_peices / 8;
        for (i = 0; i < (bytes * 8); i++) {
                if (get_bit_field(node.bit_field, i)) {
                        printf("1");
                } else {
                        printf("0");
                }
        }
        printf("\n");
}

void show() {

        int i = 0;
        int j = 0;
        struct sockaddr_in sa;
        int sa_len = sizeof(sa);
        int bytes = node.no_of_peices % 8? node.no_of_peices / 8 + 1 : node.no_of_peices / 8;

        if (global_tracker_info.peers == NULL && global_tracker_info.min_interval == 0) {
                printf("\n\n\tBe a doll and execute announce before show command \n\n");
                return;
        }
        for (i = 0; i < MAX_CONN; i++) {
                if (conn[i].in_use)
                        break;
        }

        if (i == MAX_CONN) {
                printf("******* No active connections ***********\n");
                return ;
        }
        printf("ID |   IP address/Port    | Status | Down/s |  Up/s  | Bit Field \n");
        printf("-----------------------------------------------------------------\n");
        for ( i = 0; i < MAX_CONN; i++) {
                if(conn[i].in_use) {
                        if (getsockname(conn[i].sock_fd, (struct sockaddr *) &sa, &sa_len) < 0) {
                                report_error("getsockname()");
                                continue;
                        } 
                        printf("%2d | %15s/%4d |  %d%d%d%d  | %6d | %6d | ", i, inet_ntoa(sa.sin_addr), ntohs(sa.sin_port),
                                        conn[i].am_chocking, conn[i].am_intersted, conn[i].peer_chocking, 
                                        conn[i].peer_intersted, conn[i].downspeed, conn[i].upspeed);
                        for (j = 0; j < (bytes * 8); j++) {
                                if (get_bit_field(conn[i].bit_field, j)) {
                                        printf("1");
                                } else {
                                        printf("0");
                                }
                        }
                        printf("\n");
                } 
        }
}
void * read_commands() {
        ENTER_FUNCTION
                char command_line[COMMAND_LIMIT];
        char *command;

        memset(command_line, 0, COMMAND_LIMIT);
        if (fgets(command_line, COMMAND_LIMIT, stdin) == NULL) {
                report_error("Error while reading input form the stdin\n");
        } 

        command = strtok(command_line, " \n");
        if (command != NULL) {
                if (!(strcmp(command_index[ANNOUNCE], command))) {
                        announce(1);
                        /* To avoid spawning threads whenever */
                        /* announce command was entered */
#ifdef DBG
                        printf("Establish connection with the client\n");
#endif
                        if (!seeder_flag) {
                                download_from_peers();
                        }
                } else if (!(strcmp(command_index[METAINFO], command))) {
                        display_metainfo();
                } else if (!(strcmp(command_index[TRACKERINFO], command))) {
                        display_tracker_info();
                } else if (!(strcmp(command_index[SHOW], command))){
                        show();
                } else if (!(strcmp(command_index[STATUS], command))){
                        status();
                } else if (!(strcmp(command_index[QUIT], command))){
                        announce(0);
                        exit(0);
                } else {
                        printf("Illegal command\n");
                }
        }
        printf("[ UBTorrent ]> ");

        LEAVE_FUNCTION
}

void check_seeder_status() 
{

        // TODO taking care of directory structure
        // TODO if the file exists and if its file size not 
        // equal to the size in metainfo file download the rest of the content

        struct stat st;
        int bit_field_size = 0;
        int i = 0, j = 0;
        int shift;

        if ((node.fp = fopen(node.file_name, "r")) == NULL) {
                // Unable to open the file. File does not exitst.
                // set the status to leecher status
                seeder_flag = 0;
                global_tracker_info.left = node.file_size;
                bit_field_size = (node.no_of_peices % 8) ? node.no_of_peices / 8 + 1: node.no_of_peices / 8 ;
                node.bit_field = malloc(sizeof(uint8_t) * bit_field_size);
                if (node.bit_field == NULL) {
                        sys_error("malloc()");
                }
                memset(node.bit_field, 0, bit_field_size * sizeof(uint8_t));
                node.peice_status = malloc(sizeof(int) * node.no_of_peices);
                node.peice_ranking = malloc(sizeof(sig_atomic_t) * node.no_of_peices);
                if (node.peice_status == NULL) {
                        sys_error("malloc() ");
                }
                if (node.peice_ranking == NULL) {
                    sys_error("malloc() ");
                }
                memset(node.peice_ranking, 0, sizeof(int) * node.no_of_peices);
                memset(node.peice_status, 0, sizeof(int) * node.no_of_peices);
                for (i = 0; i < node.no_of_peices; i++)
                        node.peice_status[i] = NOT_DOWNLOADED;
                node.fp = fopen(node.file_name, "w");
                if (node.fp == NULL) {
                        sys_error("Unable to open file for writing");
                }
        } else {
                // file exists. so start client in seeder state.
                stat(node.file_name, &st);
                if (node.file_size != st.st_size) {
                        printf("Error: The size of the already present file %s", node.file_name);
                        printf("does not tally with the info in .torrent file\n");
                        app_error("Remove the file and then start the ubtorrent again");
                }
                seeder_flag = 1;
                // Apply the charasterstics related to seeder
                bit_field_size = (node.no_of_peices % 8) ? node.no_of_peices / 8 + 1: node.no_of_peices / 8 ;
                node.bit_field = malloc(sizeof(uint8_t) * bit_field_size);
                if (node.bit_field == NULL) {
                        sys_error("malloc()");
                }
                memset(node.bit_field, 0, bit_field_size * sizeof(uint8_t));
                for (i = 0; i < node.no_of_peices; i++) {
                        set_bit_field(node.bit_field, i);
                }
                global_tracker_info.downloaded = node.file_size;
        }

}

void generate_peer_id(char *file_name) {
        int i = 0;
        int random_digit;
        memset(node.peer_id, 0, 20);
        for (i = 0; i < 20; i++) {
                random_digit = rand() % 16;
                node.peer_id[i] = (random_digit << 4);
                random_digit = rand() % 16;
                node.peer_id[i] = (node.peer_id[i] | random_digit);
        }
        node.metainfo_filename = file_name;
}

void loop_in_while()
{
        int nfds        =  0;
        int retval      =  0;
        int i           =  0;

        fd_set read_fds;

        while (1) {

                FD_ZERO(&read_fds);
                FD_SET(0, &read_fds);
                FD_SET(node.tcp_sockfd, &read_fds);
                nfds = node.tcp_sockfd;
                fflush(stdout);

                /* add all the established tcp connection to the fdset */
                for (i = 0; i < MAX_CONN; i++) {
                        if (conn[i].in_use) {
                                FD_SET(conn[i].sock_fd, &read_fds);
                                nfds = max(conn[i].sock_fd, nfds);
                        }
                }

                if ((retval = select(nfds + 1, &read_fds, NULL, NULL, NULL)) < 0) {
                        sys_error("select()");
                }

                if (FD_ISSET(0, &read_fds)) {
                        /* STDIN is ready with the input */
                        read_commands();
                } 

                if (FD_ISSET(node.tcp_sockfd, &read_fds)) {
                        /* TCP port is ready to read */
                        /* TCP  connection request arraived */
                        establish_tcp_connection();
                }

                /* Check if there is  any data to be read form */
                /* the already established connections */
                for (i = 0; i < MAX_CONN; i++) {
                        if(conn[i].in_use)
                                if(FD_ISSET(conn[i].sock_fd, &read_fds))
                                        read_from_client_connection(i);
                }
        }
}

int main(int argc, char **argv) 
{

        char *endptr;
        long val;
        if (argc < 3) {
                printf("Usage: ubtorrent <torrent_file_name> <port>\n");
                exit(1);
        }

        val = (uint16_t)strtol(argv[2], &endptr, 10);

        if (endptr == argv[2]) {
                printf("With an valid port number run this program again\n");
                exit(0);
        }
        if (val < 0 || val > 65535) {
                printf("Valid port numbers are between 0 and 65535\n");
                exit(0);
        }
        node.tcp_port = val;
        if (argc == 4) {
                sleep_time = atoi(argv[3]);
        }
        /* Create socket for tcp port */
        if ((node.tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                sys_error("socket() call for tcp port");
        }

        /* bind both sockets to the address */
        memset((void *) &(node.tcp_address), 0, sizeof(node.tcp_address));
        node.tcp_address.sin_family      = AF_INET;
        node.tcp_address.sin_addr.s_addr = htonl(INADDR_ANY);
        node.tcp_address.sin_port        = htons(node.tcp_port);

        /* bind server_address to the socket */
        if (bind(node.tcp_sockfd, (struct sockaddr *) &(node.tcp_address), sizeof(node.tcp_address)) < 0) {
                sys_error("bind() call for tcp");
        }

        if (listen(node.tcp_sockfd, MAX_CONN ) < 0) {
                sys_error("listen()");
        } 

        init_global_structs();

        //read metainfo file
        read_torrent_file(argv[1]);
        generate_peer_id(argv[1]);
        check_seeder_status();

        printf("[ UBTorrent ]> ");
        loop_in_while();
}
