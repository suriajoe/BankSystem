#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>


char readBuffer[256];			// char array to store data going to the server
char writeBuffer[256];			// char array to store data coming from the server
pthread_t threadHandle1;		// Handle on thread for creation and joining
pthread_t threadHandle2;		// Handle on thread for creation and joining
pthread_attr_t threadAttr;		// Hold options for thread
void* threadStatus1;			// Status of thread for join to use
void* threadStatus2;			// Status of thread for join to use
char serverStatus = 'R';		// Status of the connected server

void error(char *msg)
{
    perror(msg);
    exit(0);
}

void* read_thread(void* in_sockfd)
{
	// Unpack socket fd
	int sockfd = *((int*)in_sockfd);
	// Endlessly loop, reading in from stdin and writing to the socket
	while(serverStatus == 'R')
	{
		// Zero out buffer
		bzero(readBuffer,256);
		// Read from stdin to buffer
		fgets(readBuffer,255,stdin);
		// Wait for a bit
		sleep(2);
		// Write command to socket
		if( write(sockfd,readBuffer,strlen(readBuffer)) < 0 )
		{
			error("ERROR writing to socket");
		}		
	}
	pthread_exit(0);
}

void* write_thread(void* in_sockfd)
{
	int n = 0;
	// unpack socket fd
	int sockfd = *((int*)in_sockfd);
	// Endlessly loop, writing server responses to stdout
	while(serverStatus == 'R')
	{
		// zero out buffer
		bzero(writeBuffer,256);
		// Read from socket
		fflush(stdout);
		n = read(sockfd,writeBuffer,255);
		// Check return value
		if(n < 0)
		{
			error("ERROR reading from socket");
		}
		else if(n > 0)
		{
			if( strcmp(writeBuffer,"END") == 0)
			{
				printf("%s\n",writeBuffer);
				pthread_exit(0);
			}
			else
			{
				// Print what was read
				printf("%s\n",writeBuffer);
			}
		}
	}
	pthread_exit(0);
}

int main(int argc, char *argv[])
{
	// Declare initial vars
    int sockfd = -1;							// file descriptor for our socket
	int portno = -1;							// server port to connect to
    struct sockaddr_in serverAddressInfo;		// Super-special secret C struct that holds address info for building our socket
    struct hostent *serverIPAddress;			// Super-special secret C struct that holds info about a machine's address
	
	// If the user didn't enter enough arguments, complain and exit
    if (argc < 2)
	{
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
	
	// convert the text representation of the port number given by the user to an int
	portno = 5455;
	
	// look up the IP address that matches up with the name given - the name given might
	//    BE an IP address, which is fine, and store it in the 'serverIPAddress' struct
    serverIPAddress = gethostbyname(argv[1]);
    if (serverIPAddress == NULL)
	{
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
				
	// try to build a socket .. if it doesn't work, complain and exit
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
	{
        error("ERROR creating socket");
	}
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

	/** We now have the IP address and port to connect to on the server, we have to get    **/
	/**   that information into C's special address struct for connecting sockets                     **/

	// zero out the socket address info struct .. always initialize!
    bzero((char *) &serverAddressInfo, sizeof(serverAddressInfo));

	// set a flag to indicate the type of network address we'll be using 
    serverAddressInfo.sin_family = AF_INET;
	
	// set the remote port .. translate from a 'normal' int to a super-special 'network-port-int'
	serverAddressInfo.sin_port = htons(portno);

	// do a raw copy of the bytes that represent the server's IP address in 
	//   the 'serverIPAddress' struct into our serverIPAddressInfo struct
    bcopy((char *)serverIPAddress->h_addr, (char *)&serverAddressInfo.sin_addr.s_addr, serverIPAddress->h_length);


	//try to connect to the server using a blank socket and the address info struct
    //if it doesn't work, print error and exit. 
    //If the client does not find a server to connect with, it will repeatedly try to connect every 3 seconds

	while(connect(sockfd,(struct sockaddr *)&serverAddressInfo,sizeof(serverAddressInfo)) < 0)
	{
		if(errno == ENETUNREACH || errno == ENETDOWN || errno == EHOSTDOWN || 
             errno == EHOSTUNREACH)
          {
            //fprintf("Value of errno: %s\n",strerror(errno));
            printf("searching for server connection\n");
            sleep(3);
          }
          else
          {
            printf("searching for remote connection\n");
            sleep(3); // error("ERROR connecting\n");
          }
	}
	
	
	/**     Setup Threads for creation     **/

    // Initialize threadAttr struct
    pthread_attr_init(&threadAttr);

    // set the initialized attribute struct so that the pthreads created will be joinable
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
	
	// set the initialized attribute struct so that the pthreads created will be kernel threads
	pthread_attr_setscope(&threadAttr, PTHREAD_SCOPE_SYSTEM);

	/**     Create threads     **/
	pthread_create(&threadHandle1, &threadAttr, read_thread, (void*)&sockfd);
	pthread_create(&threadHandle2, &threadAttr, write_thread, (void*)&sockfd);
	
	// wait for the threads to finish .. make the threadStatus variables point to 
	//    the value of pthread_exit() called in each
	pthread_join( threadHandle2, NULL);

    return 0;
}