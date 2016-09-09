

#include "bank-function-lib.h"

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void* print_alarm(void* args)
{
	// Initialize semaphore to 1;
	sem_init(&sem,0,0);
	// set alarm while server is running
	while(serverStatus == 'R')
	{
		// set alarm for 20 seconds
		alarm(20);
		// now wait until
		sem_wait(&sem);
	}
	pthread_exit(0);
}


int write_to_client(int sockfd, char * buffer)
{
	// Used for write return value
	int n = -1;
	// Loop until message is recieved
	do
	{
		sleep(1);
		n = write(sockfd, buffer, strlen(buffer)+1);
	}while(n == 0);
	// Return completion status
	if(n < 0)
	{
		error("ERROR write_to_client");
	}
	else
	{
		return 1;
	}
	return -1;
}
int read_from_client(int sockfd, char * buffer)
{
	int n = 0;
	while( n == 0 )
	{
		n = read(sockfd, buffer, 255);
	}
	if(n < 0)
	{
		error("ERROR reading from socket");
	}
	return 1;
}

int open_account(char * name, account * bankAccounts[])
{
	// Counter to move through bank
	int i = 0;
	// Lock bank
	pthread_mutex_lock(&bank);
	if( bankAccounts[19] != NULL)
	{
		pthread_mutex_unlock(&bank);
		return -2;
	}
	// Search bank for account
	while(bankAccounts[i] != NULL && i<20)
	{
		// Check if name already exsists
		if( strcmp(bankAccounts[i]->name,name) )
		{
			i++;
		}
		else
		{
			pthread_mutex_unlock(&bank);
			return -1;
		}
	}
	// At this point bankAccounts[i] is pointed to a NULL spot in the bank
	bankAccounts[i] = (account*)malloc(sizeof(account));
	strcpy(bankAccounts[i]->name,name);
	bankAccounts[i]->balance = 0.0;
	bankAccounts[i]->flag = 0;
	// Unlock bank
	pthread_mutex_unlock(&bank);
	return 1;
}

int start_session(int sockfd, char * acctName, account * bankAccounts[])
{
	// Counter to move through bank
	int i = 0;
	// buffer for reading from client
	char buffer[256];
	// command to be read from buffer
	char command[20];
	// comand arg
	float arg = 0.0;
	// str to hold balance
	char fl_str[200];
	// Zero out buffers
	bzero(buffer,256);
	// lock bank
	pthread_mutex_lock(&bank);
	// Search bank for account
	for(i=0; i<20; i++)
	{
		// Check if name already exsists
		// Return error if name not found
		if( bankAccounts[i] == NULL || i == 19 )
		{
			pthread_mutex_unlock(&bank);
			return -1;
		}
		else if( strcmp(bankAccounts[i]->name,acctName) == 0 )
		{
			break;
		}
	}
	// Unlock bank
	pthread_mutex_unlock(&bank);
	if( bankAccounts[i]->flag == 1)
	{
		return -2;
	}
	else
	{
		bankAccounts[i]->flag = 1;
	}
	// account name was found
	write_to_client(sockfd,"Session started...");
	// Lock account
	pthread_mutex_lock(&acc[i]);
	while(serverStatus == 'R')
	{
        arg = 0.0;
		bzero(buffer,256);
		// Prompt user
		write_to_client(sockfd,"\nplease enter command:");
		read_from_client(sockfd,buffer);
		// attempt to read in command and arg
		sscanf(buffer,"%s %f",command,&arg);
		// check command
		if( strcmp(command,"credit") == 0 )
		{
            if ( arg == 0.0 )
            {
                write_to_client(sockfd,"Invalid argument, please try again");
            }
            else
            {
                credit(bankAccounts[i],arg);
                write_to_client(sockfd,"Funds added to account");
            }
		}
		else if( strcmp(command,"debit") == 0 )
		{
            if ( arg == 0.0 )
            {
                write_to_client(sockfd,"Invalid argument, please try again");
            }
            else if( debit(bankAccounts[i],arg) < 0 )
			{
				write_to_client(sockfd,"Not enough funds");
			}
			else
			{
				write_to_client(sockfd,"Funds deducted from account");
			}
		}
		else if( strcmp(command,"balance") == 0 )
		{
			sprintf(fl_str,"Current balance: %.2f",bankAccounts[i]->balance);
			write_to_client(sockfd,fl_str);
		}
		else if( strcmp(command,"finish") == 0 )
		{
			write_to_client(sockfd,"Ending session");
			break;
		}
		else
		{
			write_to_client(sockfd,"Invalid command");
		}
	}

	// Unlock account
	bankAccounts[i]->flag = 0;
	pthread_mutex_unlock(&acc[i]);

	return 1;
}

int credit(account * acct, float amount)
{
	// Add amount to balance
	acct->balance += amount;
	return 1;
}

int debit(account * acct, float amount)
{
	// check if valid amount
	float diff = acct->balance - amount;
	if(diff < 0)
	{
		return -1;
	}
	// if valid, perform debit
	else
	{
		acct->balance -= amount;
	}
	return 1;
}

pthread_t * insert_thread(node * head)
{
	node * newNode = (node*)malloc(sizeof(node));
	if(newNode == NULL)
	{
		printf("====MALLOC FAIL====\n");
	}
	newNode->next = head;
	head = newNode;

	// Test print
	int i = 0;
	newNode = head;
	while(newNode != NULL)
	{
		printf("found node\n");
		newNode = newNode->next;
		i++;
	}
	printf("Found %d nodes\n",i);
	return (pthread_t *)head;
}

void join_threads(node * head)
{
	int i = 0;
	node * currentNode = head;
	while(currentNode != NULL)
	{
		printf("Joining on thread: #%d\n",i);
		if( pthread_join( currentNode->handle , NULL) < 0 )
			printf("[Fail on join]\n");
		currentNode = currentNode->next;
		i++;
	}
}

void destroy_list(node * head)
{
	int i = 0;
	node * currentNode = head;
	while(head != NULL)
	{
		while(currentNode->next != NULL)
		{
			currentNode = currentNode->next;
		}
		printf("freeing node: #%d\n",i);
		free(currentNode);
		currentNode = head;
		i++;
	}
}

void destroy_bank(account * bankAccounts[])
{
	int i = 0;
	for(i = 0; i < 20; i++)
	{
		if(bankAccounts[i] != NULL)
		{
			printf("Freeing account: #%d\n",i);
			free(bankAccounts[i]);
		}
	}
}
