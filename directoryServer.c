#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h>  


struct entry
{
    
    char *topicName;
	int localport;
	int ip;
	int number;

    LIST_ENTRY(entry) entries;
};

LIST_HEAD(listhead, entry);


int main(int argc, char **argv)
{
	char s[MAX] = {'\0'};
	fd_set			readset;
	int				sockfd, newsockfd;
	struct sockaddr_in serv_addr, chat_serv_addr;
	int				nread;	/* number of characters */
	char message[MAX] = {'\0'};
	int validusername = 0;
	struct entry * n1,*np, *end;
	int local_port;
	int ip_address;


	if(argc == 1){
	/* Set up the address of the server to be contacted. */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family			= AF_INET;
	serv_addr.sin_addr.s_addr	= inet_addr(SERV_HOST_ADDR);
	serv_addr.sin_port			= htons(SERV_TCP_PORT);

	/* Create a socket (an endpoint for communication). */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		exit(1);
	}
	//first make a connection to the directory server, send a request for the names, print out those names.
	//what we do is based on the number of arguments that the user passes in 
	// we need the serv_addr , it's sin_family...., ip addr and port


	/* Connect to the server. */
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("client: can't connect to server");
		exit(1);
	}
			//ask the server for a list of servers
			//display the list
			//then shutdown. then the server can 
	printf("List of servers to connect to.\n");
	write(sockfd,"c",MAX);
	struct listhead head;
	LIST_INIT(&head);
	end = malloc(sizeof(struct entry));
	LIST_INSERT_HEAD(&head, end, entries);


	int k = 0;

	while(1){
		if ((nread = read(sockfd, s, MAX)) <= 0) {
			printf("Error reading from server\n");
			exit(1);
		} else {
			if(strncmp(s,"DONE",4) ==0){
				break;
			}
			printf("Read from server: %s\n", s);
			n1 = malloc(sizeof(struct entry));
			n1->topicName = (char *)malloc(50 * sizeof(char));
			if (n1->topicName == NULL)
				{
					fprintf(stderr, "Memory allocation failed\n");
					exit(1);
				}
			n1->number = k;
			k += 1;
			fprintf(stderr,"%d\n",k);
			if(3 == sscanf(s,"%d %s %d\n",&(n1->ip),n1->topicName,&(n1->localport))){

			LIST_INSERT_BEFORE(end,n1,entries);
				
			}
			else{
				fprintf(stderr,"error parsing server response");
			}			
		}
	}

	fprintf(stderr, "Type in the number of the topic of the chat topic you want to join\n");
		fprintf(stderr,"printing those in the list\n");
		LIST_FOREACH(np, &head, entries)
		{		
			if(np->topicName !=NULL){				
		fprintf(stderr,"%d : %s %d %d\n",np->number, np->topicName,np->ip,np->localport);
			}
		}
		int choice;
		int found = 0;

		if(scanf(" %1d", &choice) == 1){
			fprintf(stderr,"scanf worked\n");
			fprintf(stderr,"choice %d\n",choice);

				LIST_FOREACH(np, &head,entries){
					if(np->topicName == NULL) continue;
					fprintf(stderr,"np->number %d\n",np->number);
					if (np->number == choice){
						fprintf(stderr,"found it\n");
						//set address and portal to connect to 
						local_port = np->localport;
						fprintf(stderr,"localport\n");
						ip_address = np->ip;
						fprintf(stderr,"ip_address\n");
						found = 1;
						fprintf(stderr,"found\n");
						break;
					}
				}
		}				
		if(0 == found) {
			printf("wrong input");
			exit(1);
		}
		fprintf(stderr,"reached this point\n");
		int c;
		while ((c = getchar()) != '\n' && c != EOF);
 fprintf(stderr, "%s:%d Before setting up address\n", __FILE__, __LINE__);


	/* Set up the address of the server to be contacted. */
	memset((char *) &chat_serv_addr, 0, sizeof(chat_serv_addr));
	chat_serv_addr.sin_family			= AF_INET;
	//expecting a string
	//local_port= ntohl(cli_addr.sin_addr.s_addr);
	chat_serv_addr.sin_addr.s_addr	= htonl(ip_address);
	chat_serv_addr.sin_port			= htons(local_port);

	fprintf(stderr,"%s");
	/* Create a socket (an endpoint for communication). */
	if ((newsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		exit(1);
	}
	//first make a connection to the directory server, send a request for the names, print out those names.
	//what we do is based on the number of arguments that the user passes in 
	// we need the chat_serv_addr , it's sin_family...., ip addr and port

		char *lp = inet_ntoa(chat_serv_addr.sin_addr);

		// Print the IP address
		if (lp != NULL) {
			printf("Client IP address: %s\n", lp);
		} else {
			printf("Error converting IP address.\n");
		}
		fprintf(stderr,"server port %d\n",ntohs(chat_serv_addr.sin_port));

	/* Connect to the server. */
	if (connect(newsockfd, (struct sockaddr *) &chat_serv_addr, sizeof(chat_serv_addr)) < 0) {
		perror("client: can't connect to server");
		exit(1);
	}


	//now connect to the server we got
	fprintf(stderr,"messages must be at most 74 characters long or will be truncated by the program.\n");
	fprintf(stderr,"Enter a name to join\n");




	//parse the command line arguments.
    memset(s,'\0',MAX);
	for(;;) {

		FD_ZERO(&readset);
		FD_SET(STDIN_FILENO, &readset);
		FD_SET(newsockfd, &readset);

		if (select(newsockfd+1, &readset, NULL, NULL, NULL) > 0)
		{
			/* Check whether there's user input to read */
			if (FD_ISSET(STDIN_FILENO, &readset)) {
				if(validusername == 1){
					if(1 == scanf(" %76s",s)){
										int c;
				while ((c = getchar()) != '\n' && c != EOF);
						snprintf (message,MAX,"m%s",s);
					}
					else{
						printf("Error reading or parsing user input\n");
					}
				if(s[75]!='\0'){
					printf("Input too long. Will be truncated. You may send another message.\n");
					s[75] = '\0';
				}
					write(newsockfd, message, MAX);
				}
				else{					
					if(1 == scanf(" %10s",s)){
										int c;
				while ((c = getchar()) != '\n' && c != EOF);
						//REJECT EMPTY NAMES TOO
						
						//write(sockfd, message, MAX);
												
					if(s[8]!='\0'){
						printf("%s %d",s,strlen(s));
						printf("Name is too long.Try again. 8 characters only\n");
						printf("Name: ");
						s[8] = '\0';
					}
					else{
						validusername = 1;
						snprintf (message,MAX,"n%s",s);
					write(newsockfd, message, MAX);
					}
					}
					else{
						printf("Error reading or parsing user input\n");
					}		
					//scanf partial matches,never assume scanf matched properly 
				}
				int c;
				while ((c = getchar()) != '\n' && c != EOF);
				printf("next loop");

			}
			/* Check whether there's a message from the server to read */
			if (FD_ISSET(newsockfd, &readset)) {
				if ((nread = read(newsockfd, s, MAX)) <= 0) {
					printf("Error reading from server\n");
					exit(1);
				} else {
					printf("Read from server: %s\n", s);
					if (strncmp(s, "Duplicate name, already taken\n", 31) == 0)
						{	
						validusername = 0;
						printf("Duplicate name.Enter a name to join\n");
						}
				}
			}
		}
	}
	close(sockfd);
	}
}
