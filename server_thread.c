#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include "header.h"

int port = 0;
int socket_fd;
pthread_t motherthread;
account *directory;

pthread_mutex_t directory_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct all_sockets{
	int socket_id;
	struct all_sockets *next;
}all_sockets;
all_sockets *mothersocket;
sem_t s;

struct sigaction sa; 
struct itimerval timer;


void timer_handler(int signum)
{
  sem_wait(&s);
  printf("\n********** ACCOUNTS **********\n\n");
  account *temp = directory;
  while(temp)
  {
    if(temp->isInSession == 0)
      printf("%s\t%f\t\n", temp->account_name, temp->balance);
    else
      printf("%s\t%f\tIN SERVICE\n",temp->account_name, temp->balance);
    temp = temp->next;
    
  }
  printf("\nDONE PRINTING LIST OF ACCOUNTS\n");
  
  sem_post(&s);
}

int main(int argc, char *argv[])
{
	port = argc > 1 ? atoi(argv[1]) : 0;
	if (!port) 
	{
		perror("no port entered\n");
		exit(1);
	}
  
	sem_init(&s, 0, 1);
  
  /* Install timer_handler as the signal handler for SIGVTALRM. */
 memset (&sa, 0, sizeof (sa));
 sa.sa_handler = &timer_handler;
 sigaction (SIGALRM, &sa, NULL);

 /* Configure the timer to expire after 250 msec... */
 timer.it_value.tv_sec = 15;
 timer.it_value.tv_usec = 0;
 /* ... and every 250 msec after that. */
 timer.it_interval.tv_sec = 15;
 timer.it_interval.tv_usec = 0;
 /* Start a virtual timer. It counts down whenever this process is
   executing. */
 setitimer (ITIMER_REAL, &timer, NULL);
  
  
	if (pthread_create(&motherthread, NULL, connection_handler, NULL) != 0)
		perror("could not spawn motherthread");
	pthread_join(motherthread, NULL);
	return 0;
}

void* connection_handler()
{
  int new_socket_fd;
	struct sockaddr_in address;
	pthread_t new_client;
	socklen_t clilen;
	client *client_arg;
	
	 /* Initialise IPv4 address. */
   memset(&address, 0, sizeof address);
   address.sin_family = AF_INET;
   address.sin_port = htons(port);
   address.sin_addr.s_addr = INADDR_ANY;
	
	 /* Create TCP socket. */
   if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }

	int enable = 1;
	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    error("setsockopt(SO_REUSEADDR) failed");
   /* Bind address to socket. */
   if (bind(socket_fd, (struct sockaddr *)&address, sizeof address) == -1) {
        perror("bind");
        exit(1);
    }

   /* Listen on socket. */
   if (listen(socket_fd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
		
		/* Assign signal handlers to signals. */

    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal");
        exit(1);
    }
	
		while(1)
		{
			client_arg = (client*)malloc(sizeof *client_arg);
			if (!client_arg) {
            perror("malloc client_arg");
            continue;
			}
			/* Accept connection to client. */
        clilen = sizeof client_arg->client_address;
			
        new_socket_fd = accept(socket_fd, (struct sockaddr *)&client_arg->client_address, &clilen);
        if (new_socket_fd == -1) {
            perror("accept");
            free(client_arg);
            continue;
        }
			
				all_sockets *add_socket = (all_sockets*)malloc(sizeof(all_sockets));
				add_socket->socket_id = new_socket_fd;
				if(mothersocket == NULL)
					mothersocket = add_socket;
				else
				{
					add_socket->next = mothersocket;
					mothersocket = add_socket;
				}
			
				/* Initialise pthread argument. */
        client_arg->new_socket_fd = new_socket_fd;
			
			/* Create thread to serve connection to client. */
        if (pthread_create(&new_client, NULL, pthread_routine, (void *)client_arg) != 0) {
            perror("pthread_create");
            free(client_arg);
            continue;
        }
				pthread_detach(new_client);
		}
}

void *pthread_routine(void *arg) {
    client *pthread_arg = (client *)arg;
    int new_sock_fd = pthread_arg->new_socket_fd;
    struct sockaddr_in client_address = pthread_arg->client_address;
		printf("[+]Client Connected: %s\n", inet_ntoa(client_address.sin_addr));
    free(arg);
	char buff[1024];
	int quitFlag = 1;
	while(quitFlag == 1)
	{
		fflush(stdout);
		bzero(buff, sizeof(buff));
		recv(new_sock_fd, buff, sizeof(buff), 0);
		int size_buff = strlen(buff); //326 is max
		char* cmd = NULL;
		char* args = NULL;
    checkInput(buff, &cmd, &args);
		if(strncmp(cmd, "quit", 4) == 0)
		{
			//printf("[-]Client Disconnected1\n");
			//send(new_sock_fd, "quit", 4, 0);
			quitFlag = 0;
			continue;
    }
		else if(size_buff > 326)
			send(new_sock_fd, "inv_len", 7, 0);
		else if(strcmp(cmd, "create ") == 0)
		{
			if(args == NULL || strlen(args) > 255) // if invalid arg name
				send(new_sock_fd, "err: missing/invalid account name",33 , 0);
			else // check check if account already exists, if not->add it
			{
        pthread_mutex_lock(&directory_mutex);
				//if directory is NULL
				account *new_client = (account*)malloc(sizeof(account));
				new_client->account_name = strdup(args);
				new_client->balance = 0.0;
				new_client->isInSession = 0;
				new_client->next = NULL;
				if(directory == NULL)
				{
					directory = new_client;
					send(new_sock_fd, "ack: Account Created [+]",25 , 0);
				}
				else{
					account *temp = directory;
					int isThere = 0;
					while(temp)
					{
						if(strcmp(temp->account_name, args) == 0)
						{
							send(new_sock_fd, "err: Cannot Create Account, Duplicate",37, 0);
							isThere = 1;
							break;
						}
						temp = temp->next;
					}
					if(isThere != 1)
					{
						new_client->next = directory;
						directory = new_client;
						send(new_sock_fd, "ack: Account Created [+]",25 , 0);
					}
          else if(isThere == 1)
            free(new_client);
				}
        pthread_mutex_unlock(&directory_mutex);
			}
		}
		else if(strcmp(cmd, "serve ") == 0)
		{
			if(args == NULL || strlen(args) > 255) // if invalid arg name
				send(new_sock_fd, "err: missing/invalid account name",33 , 0);
			else{
				//find the account
				int found_account = 0;
				account *acc_to_serve = directory;
				while(acc_to_serve)
				{
					if(strcmp(acc_to_serve->account_name, args) == 0)
						break;
					acc_to_serve = acc_to_serve->next;
				}
				if(acc_to_serve == NULL)
					send(new_sock_fd, "err: account does not exist", 27,0);
				else if(acc_to_serve->isInSession == 1)
				{
					send(new_sock_fd, "err: Account already in service", 31,0);
					continue;
				}
				else if(acc_to_serve->isInSession == 0)
				{
					send(new_sock_fd, "ack: found account to serve", 27,0);
					acc_to_serve->isInSession = 1;
					while(acc_to_serve->isInSession == 1)
					{
						char serve_cmd[400];
						bzero(serve_cmd, 400);
						recv(new_sock_fd, serve_cmd, sizeof(serve_cmd), 0);
            char* cmd1 = NULL;
						char* args1 = NULL;
            checkInput(serve_cmd, &cmd1, &args1);
						if(strncmp(cmd1, "end",3) == 0)
						{
							send(new_sock_fd, "ack: ended service session", 26,0);
							acc_to_serve->isInSession = 0;
							break;
						}
						else if(strncmp(cmd1, "query",5) == 0)
						{
							char value[320];
							snprintf(value, sizeof(value), "%f", acc_to_serve->balance);
							send(new_sock_fd, value, strlen(value),0);
						}
						else if(strncmp(cmd1, "quit",4) == 0)
						{
							acc_to_serve->isInSession = 0;
							//send(new_sock_fd, "ack: Ended Service Session and Quit", 35,0);
							//send(new_sock_fd, "quit", 4, 0);
							quitFlag = 0;
							break;
						}
						else if(strncmp(serve_cmd, "create ", 7) == 0)
						{
							send(new_sock_fd, "err: Cannot create account, end session first", 45,0);
						}
						else if(strncmp(serve_cmd, "serve ", 6) == 0)
						{
							send(new_sock_fd, "err: Cannot serve another account, end session first", 52,0);
						}
            else if(strcmp(cmd1, "withdraw ") == 0)
            {
              if(args1 == NULL || !(atof(args1)) || atof(args1) > acc_to_serve->balance || atof(args1) < 0)
                send(new_sock_fd, "err: invalid amount to withdraw", 72,0);
              else
              {
                acc_to_serve->balance = acc_to_serve->balance - atof(args1);
                send(new_sock_fd, "ack: withdraw success", 21,0);
              }
            }
            else if(strcmp(cmd1, "deposit ") == 0)
            {
              if(args1 == NULL || !(atof(args1)) || atof(args1) < 0)
                send(new_sock_fd, "err: invalid amount to deposit", 72,0);
              else
              {
                acc_to_serve->balance = acc_to_serve->balance + atof(args1);
                send(new_sock_fd, "ack: deposit success", 20,0);
              }
            }
					}
				}
			}
		}
		else
			send(new_sock_fd, "err: cannot execute command",27 , 0);
		
		bzero(buff, sizeof(buff));
	}
	if(quitFlag == 0)
	{
			printf("[-]Client Disconnected\n");
			send(new_sock_fd, "quit", 4, 0);
	}
    close(new_sock_fd);
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
		if(in[strlen(in)-1] == '\n')
		  in[strlen(in)-1] = '\0';;
		if(strcmp(in, "query") == 0 || strcmp(in, "quit") == 0 || strcmp(in, "end") == 0)
			*cmd = in;
		return;
	}

    char* dup = strdup(in);
    i = 0;
    while(in[i] != ' ')
        i++;
		i++;
    in = in+i;
    in[strlen(in)-1] = '\0';
    if(strlen(in) <= 255 && strlen(in) > 0)
        *arg = in;

    dup[i] = '\0';
	if(strcmp(dup, "create ") == 0 || strcmp(dup, "serve ") == 0 || strcmp(dup, "deposit ") == 0 || strcmp(dup, "withdraw ") == 0)
		*cmd = dup;
	fflush(stdout);
	return;
}

void signal_handler(int signal_number) {
    /* TODO: Put exit cleanup code here. */
     printf("\nSERVER IS SHUTTING DOWN...\n");
		all_sockets *temp = mothersocket;
    //free the linked list
    account *current = directory;
    while (current)
    {
      account *next = current->next;
      free(current);
      current = next;
    }
		while(temp)
		{
			send(temp->socket_id, "shut", 4, 0);
			close(temp->socket_id);
			temp = temp->next;
		}
		pthread_join(motherthread, NULL);
    printf("\nSERVER SHUT DOWN\n");
    close(socket_fd);
		exit(0);
}
