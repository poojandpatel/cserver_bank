#ifndef header_h
#define header_h
#define BACKLOG 256
#define SERVER_NAME_LEN_MAX 255

typedef struct pthread_arg_t {
    int new_socket_fd;
    struct sockaddr_in client_address;
    /* TODO: Put arguments passed to threads here. See lines 116 and 139. */
} client;

typedef struct account {
    char *account_name;
	  double balance;
		int isInSession; // 1 if yes, 0 if no
		int socket;
		struct account *next;
} account;

/* Thread routine to serve connection to client. */
void *pthread_routine(void *arg);

/* Signal handler to handle SIGTERM and SIGINT signals. */
void signal_handler(int signal_number);

void* connection_handler();
void checkInput(char* in, char** cmd, char** arg);
void* send_cmd(void* input_socket1);
void* recv_out(void* socket_fd1);
void timer_handler(int signum);

#endif