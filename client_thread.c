
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "header.h"

#define SERVER_NAME_LEN_MAX 255
struct sockaddr_in server_address;

pthread_t send_cmdthread;
		pthread_t receive_cmdthread;
int socket_fd;
int main(int argc, char *argv[]) {
    
    char server_name[SERVER_NAME_LEN_MAX + 1] = { 0 };
    int server_port;
    struct hostent *server_host;
    //struct sockaddr_in server_address;
		
    /* Get server name from command line arguments or stdin. */
    if (argc == 3) {
        strncpy(server_name, argv[1], SERVER_NAME_LEN_MAX);
    } else {
        fprintf(stderr,"missing parameters\n");
        return -1;
    }

    /* Get server port from command line arguments or stdin. */
    server_port = argc > 2 ? atoi(argv[2]) : 0;
    if (!server_port) {
        printf("Enter Port: ");
        scanf("%d", &server_port);
    }

    /* Get server host from server name. */
    server_host = gethostbyname(server_name);
		
    /* Initialise IPv4 server address with server host. */
    memset(&server_address, 0, sizeof server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    memcpy(&server_address.sin_addr.s_addr, server_host->h_addr, server_host->h_length);

    /* Create TCP socket. */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

    /* Connect to socket with server address. */
    while (connect(socket_fd, (struct sockaddr *)&server_address, sizeof server_address) == -1) {
				sleep(3);
		}
		printf("[+]Connected to Server.\n");
		
		if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
	
		pthread_create(&send_cmdthread, NULL, send_cmd, (void*) &socket_fd);
		pthread_create(&receive_cmdthread, NULL, recv_out, (void*) &socket_fd);
	
	
		pthread_join(receive_cmdthread, NULL);	
		pthread_join(send_cmdthread, NULL);
	
	
		
		
    /* TODO: Put server interaction code here. For example, use
     * write(socket_fd,,) and read(socket_fd,,) to send and receive messages
     * with the client.
     */
		//commands(socket_fd);
		
    close(socket_fd);
    return 0;
}

void* send_cmd(void* input_socket1)
{
  int input_socket = *((int*) input_socket1);
	char buff[1024];
	bzero(buff, 1024);
	while(1)
	{
		//only allow input to be less than certain size
		int n = 0;
		bzero(buff, 1024);
		while((buff[n++] = getchar()) != '\n')
			;
		char* cmd= NULL;
    char* x = NULL;
    checkInput(buff,&cmd, &x);
		
		if(strncmp(cmd, "INVALID", 7) == 0)
		{
			printf("\nInvalid Command Format, Use Following Commands:\n");
			printf("\tcreate <account name>\n\tserve <account name>\n");
			printf("\tdeposit <amount>\n\twithdraw <amount>\n");
			printf("\tquery\n\tend\n\tquit\n");
		}
		else if(strncmp(cmd, "quit", 4) == 0)
		{
			send(socket_fd, "quit", 4, 0);
			break;
		}
		else
		{
			send(socket_fd, buff, strlen(buff), 0);
			sleep(2);
		}
		bzero(buff, 1024);
	}
	pthread_exit(NULL);
}

void* recv_out(void* socket_fd1)
{
   int socket_fd = *((int*) socket_fd1);
	char buff[1024];
	while(1)
	{
		recv(socket_fd, buff, 1024, 0);
		if(strcmp(buff, "quit") == 0)
		{
			printf("[-]Disconnecting From Server\n");
			//pthread_exit(NULL);
			break;
		}
		else if(strcmp(buff, "shut") == 0)
		{
			printf("[-]Server Shutdown\n");
			pthread_cancel(send_cmdthread);
			break;
		}
		else if(strcmp(buff, "create") == 0)
		{
			printf("[+]Account Created by Server\n");
		}
		else if(strcmp(buff, "inv_len") == 0)
		{
			printf("[-]Invalid command length\n");
		}
		else if(strstr(buff,"err") != NULL)
		{
			printf("%s\n", buff);
		}
		else if(strstr(buff,"ack") != NULL)
		{
			printf("%s\n", buff);
		}
    else if(strlen(buff) > 0)
      printf("balance: %s\n", buff);
		bzero(buff, sizeof(buff));
	}
	pthread_exit(NULL);
}

void checkInput(char* in, char** cmd, char** arg)
{
    // check if more than one space, if so goto else
    int count = 0;
    int i = 0;
    for(i = 0; i < strlen(in);i++)
		in[i] == ' ' && count++;
	
	if(count == 0)
	{
		in[strlen(in)-1] = '\0';
		if(strcmp(in, "quit") == 0 || strcmp(in, "query") == 0 || strcmp(in, "end") == 0)
			*cmd = in;
		else
			*cmd = "INVALID";
		return;
	}

    char* dup = strdup(in);
    i = 0;
    while(in[i] != ' ')
        i++;
		i++;
    in = in+i;
    in[strlen(in)+1] = '\0';
    if(strlen(in) <= 256 && strlen(in) > 0)
        *arg = in;

    dup[i] = '\0';
	if(strcmp(dup, "create ") == 0 || strcmp(dup, "serve ") == 0 || strcmp(dup, "deposit ") == 0 || strcmp(dup, "withdraw ") == 0)
		*cmd = dup;
	else
		*cmd = "INVALID";
	fflush(stdout);
	return;
}

void signal_handler(int signal_number) {
	send(socket_fd, "quit", 4, 0);
	exit(0);
}

