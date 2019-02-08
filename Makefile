all: bankingServer bankingClient

bankingServer: server_thread.c
	gcc -pthread server_thread.c -o bankingServer
bankingClient: client_thread.c
	gcc -pthread client_thread.c -o bankingClient
clean:
	rm -rf bankingClient bankingServer