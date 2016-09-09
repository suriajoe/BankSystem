#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <semaphore.h>

struct account_
{
	int flag;
	char name[100];
	float balance;
};
typedef struct account_ account;

struct args
{
	int newsockfd;
	account * BankAccounts[20];
};

typedef struct node_ node;
struct node_{
	pthread_t handle;
	node * next;
};

sem_t sem;
static pthread_mutex_t bank;
static pthread_mutex_t acc[19];
static char serverStatus = 'R';

void error(char *msg);

void* print_alarm(void* args);

int write_to_client(int sockfd, char * buffer);

int read_from_client(int sockfd, char * buffer);

int open_account(char * name, account * bankAccounts[]);

int start_session(int sockfd, char * acctName, account * bankAccounts[]);

int credit(account * acct, float amount);

int debit(account * acct, float amount);

pthread_t * insert_thread(node * head);

void join_threads(node * head);

void destroy_list(node * head);

void destroy_bank(account * bankAccounts[]);