#include <netinet/tcp.h>
#include "bank-function-lib.h"

/**     Data Segment Stuff     **/
int sockfd;								// file descriptor for our server socket
int portno;								// server port to connect to
int clilen;								// utility variable - size of clientAddressInfo below
pthread_t * threadHandle;				// Handle on thread for creation and joining
pthread_t alarmThreadHandle;			// Handle on alarm setting thread
pthread_t shutdownHandle;				// Handle on shutdown thread
pthread_attr_t threadAttr;				// Hold options for thread
void* threadStatus;						// Status of thread for join to use
struct args * threadArgs;				// Args for start_comm threads
struct sockaddr_in serverAddressInfo;	// Super-special secret C struct that holds address info for building our server socket
struct sockaddr_in clientAddressInfo;	// Super-special secret C struct that holds address info about our client socket
account ** G_bank;						// Global pointer to bank for printing and destroying
node * head;							// Head node of handler list
sem_t shutdownSem;						// sem to block shutdown server thread



void print_server_status(int signum)
{
	pthread_mutex_lock(&bank);
	// Counter to move through bank
	int i = 0;
	// Search bank for account
	printf("\n\n====== Bank Status ======\n");
	while(G_bank[i] != NULL && i<20)
	{
		printf("Account #%d:\n",i);
		printf("Account holder: %s\n",G_bank[i]->name);
		if(G_bank[i]->flag == 1)
		{
			printf("IN SESSION\n");
		}
		else
		{
			printf("balance: %.2f\n",G_bank[i]->balance);
		}
		i++;
	}
	printf("========== End ==========\n\n");
	// post to semaphore to start timer over again
	if( sem_post(&sem) < 0 )
	{
		printf("Failed to post\n");
	}
	pthread_mutex_unlock(&bank);
	return;
}

void sigint_handler(int signum)
{
	printf("\nStarting shutdown proccedure...\n");
	// Set server status to shutdown
	serverStatus = 'S';
	sem_post(&shutdownSem);
}

void* shutdown_server(void* args)
{
	// Wait until signal is thrown
	sem_wait(&shutdownSem);

	// join on all threads
	printf("Joining threads...\n");
	join_threads(head);
	pthread_attr_destroy(&threadAttr);

	// free list
	printf("Destroying list...\n");
	destroy_list(head);

	// Destroy bank
	printf("Destroying bank...\n");
	destroy_bank(G_bank);

	// Destroy threadarg
	printf("Free args...\n");
	free(threadArgs);

	// end execution
	printf("====== TERMINATE ======\n");
	exit(0);
}


void* start_comm(void* ThreadArgs)
{
	// Unpack arguments struct
	account ** BankAccounts = ((struct args * )ThreadArgs)->BankAccounts;
	int socketfd = ((struct args * )ThreadArgs)->newsockfd;

	// setup buffer
	char buffer[256];
	bzero(buffer,256);

	// setup command and arg
	char command[20];
	char arg[20];
	int retVal = 0;

	// Tell client that server is ready for command
	write_to_client(socketfd,"Hello! Welcome to Banky Bank!");
	while(serverStatus == 'R')
	{
        // Zero and prep
		bzero(buffer,256);
        bzero(arg,20);

		// Prompt user
		write_to_client(socketfd,"\nPlease enter command:");

		// try to read from the client socket
	    read_from_client(socketfd,buffer);
		
		// Print msg
		printf("Msg recived:\n");

		// unpack msg
		sscanf(buffer,"%s %s",command,arg);
		printf("command: %s\n", command);
		printf("arg: %s\n", arg);
		
		// Execute command
		if( strcmp(command,"open") == 0 )
		{
            // Check arg was valid
            if( arg == NULL )
            {
                write_to_client(socketfd,"Invalid argument, please try again");
            }
			// Check that operation was successful
            else
			{
				retVal = open_account(arg,BankAccounts);
				if( retVal == -1 )
				{
					write_to_client(socketfd,"Account already exsits, please try again");
				}
				else if( retVal == -2 )
				{
					write_to_client(socketfd,"All accounts open, not accepting new clients");
				}
				else
				{
					write_to_client(socketfd,"successfully opened account");
				}
			}
		}
		else if( strcmp(command,"start") == 0 )
		{
            // Check arg was valid
            if( arg == NULL )
            {
                write_to_client(socketfd,"Invalid argument, please try again");
            }
			// Check is account exisits
            else
			{
				retVal = start_session(socketfd,arg,BankAccounts);
				if( retVal == -1 )
				{	
					write_to_client(socketfd,"Account not found, Please try again");
				}
				else if( retVal == -2 )
				{
					write_to_client(socketfd,"Account already in session, Try again later");
				}
			}
		}
		else if( strcmp(command,"credit") == 0 || 
					strcmp(command,"debit") == 0 ||
					strcmp(command,"balance") == 0 ||
					strcmp(command,"finish") == 0 )
		{
			write_to_client(socketfd,"Session not open, Please open session and try again");
		}
		else if( strcmp(command,"exit") == 0 )
		{
			write_to_client(socketfd,"Thank you!");
			write_to_client(socketfd,"END");
			pthread_exit(0);
		}
		else
		{
			write_to_client(socketfd,"Invalid command, please try again");
		}
	}
	write_to_client(socketfd,"Server Shuting down");
	write_to_client(socketfd,"END");
	printf("Exiting thread...\n");
	pthread_exit(0);
}


int main(int argc, char *argv[])
{
	/**     Setup signals     **/
	signal(SIGALRM,print_server_status);
	signal(SIGINT,sigint_handler);
	
	/**     Setup mutex     **/
	if (pthread_mutex_init(&bank, NULL) != 0)
    {
        printf("\n mutex init failed\n");
        return 1;
    }
    int i = 0;
    for(i = 0; i < 20; i++)
    {
	    if (pthread_mutex_init(&acc[i], NULL) != 0)
	    {
	        printf("\n mutex init failed\n");
	        return 1;
	    }
	}

	/**     Setup Server socket     **/

	// convert the text representation of the port number given by the user to an int  
	portno = 5455;
	 
	// try to build a socket .. if it doesn't work, complain and exit
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
	{
       error("ERROR opening socket");
	}

	// Set socket options
    int a = 1;
    struct linger so_linger;
    so_linger.l_onoff = a;
    so_linger.l_linger = 30;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &a, sizeof(a)) < 0)
    	error("setsockopt(SO_REUSEADDR) failed");
	if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger)) < 0)
    	error("setsockopt(SO_LINGER) failed");
	if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &a, sizeof(a)) < 0)
    	error("setsockopt(TCP_NODELAY) failed");

	/** We now have the port to build our server socket on .. time to set up the address struct **/

	// zero out the socket address info struct .. always initialize!
	bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));

	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(portno);
	 
	// set a flag to indicate the type of network address we'll be using  
    serverAddressInfo.sin_family = AF_INET;
	
	// set a flag to indicate the type of network address we'll be willing to accept connections from
    serverAddressInfo.sin_addr.s_addr = INADDR_ANY;


	/** We have an address struct and a socket .. time to build up the server socket **/
     
    // bind the server socket to a specific local port, so the client has a target to connect to      
    if (bind(sockfd, (struct sockaddr *) &serverAddressInfo, sizeof(serverAddressInfo)) < 0)
	{
		error("ERROR on binding");
	}
			  
	// set up the server socket to listen for client connections
    listen(sockfd,5);
	
	// determine the size of a clientAddressInfo struct
    clilen = sizeof(clientAddressInfo);


    /**     Setup Thread for creation     **/

    // Initialize threadAttr struct
    pthread_attr_init(&threadAttr);

    // set the initialized attribute struct so that the pthreads created will be joinable
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
	
	// set the initialized attribute struct so that the pthreads created will be kernel threads
	pthread_attr_setscope(&threadAttr, PTHREAD_SCOPE_SYSTEM);

    /**     Build Thread arg struct     **/

    // Mallco thread arg struct
    threadArgs = (struct args * )calloc(1,sizeof(struct args));

    /**     create alarm thread     **/
    pthread_create(&alarmThreadHandle, &threadAttr, print_alarm, 0);
    // set global pointer to bank for alarm thread to use
    G_bank = threadArgs->BankAccounts;

    /**     setup and run shutdown thread    **/
    sem_init(&shutdownSem,0,0);
    // This thread will wait until a SIGINT posts to shutdownSem
    pthread_create(&shutdownHandle, &threadAttr, shutdown_server, 0);

    // Set list head to NULL
    head = NULL;
	
	while(serverStatus == 'R')
	{
		// block until a client connects, when it does, create a client socket
        printf("\nWaiting for new connection...\n");
	    threadArgs->newsockfd = accept(sockfd, (struct sockaddr *) &clientAddressInfo, &clilen);

   	    //Create new thread...
        printf("\nFound new connection...\n");
        threadHandle = insert_thread(head);
		pthread_create(threadHandle, &threadAttr, start_comm, (void*)threadArgs);
	}

	printf("Exiting main...\n");

	return 0;
}
