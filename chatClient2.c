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
	write(sockfd,"c",2);
	struct listhead head;
	LIST_INIT(&head);
	end = malloc(sizeof(struct entry));
	LIST_INSERT_HEAD(&head, end, entries);

	int k = 1;

	while(1){
		//
				
		if ((nread = read(sockfd, s, MAX)) == 0) {

			break;
		}
		else if(nread <0){
			printf("Error reading from server\n");
			exit(1);
		} 
		else {
			// if(strncmp(s,"DONE",4) ==0){
			// 	break;
			// }
			n1 = malloc(sizeof(struct entry));
			n1->topicName = (char *)malloc(50 * sizeof(char));
			if (n1->topicName == NULL)
				{
					fprintf(stderr, "Memory allocation failed\n");
					exit(1);
				}
			n1->number = k;
			k += 1;
			//fprintf(stderr,"%d\n",k);
			if(3 == sscanf(s," %d %s %d\n",&(n1->ip),n1->topicName,&(n1->localport))){

			LIST_INSERT_BEFORE(end,n1,entries);
				
			}
			else{
				fprintf(stderr, "%s",s);
				fprintf(stderr,"error parsing server response\n");
			}			
		}
		
	}
	//if there are no servers to connect to 
	if(n1 ==NULL){
		fprintf(stderr,"No servers available\n");
		exit(0);
	}

		printf("List of servers to connect to.\n");


		LIST_FOREACH(np, &head, entries)
		{		
			if(np->topicName !=NULL){	
			fprintf(stderr,"Number %d : %s\n",np->number, np->topicName);
			
		// fprintf(stderr,"Number %d : %s %d %d\n",np->number, np->topicName,np->ip,np->localport);
			}
		}

	fprintf(stderr, "Type in the number of the topic of the chat topic you want to join\n");

		int choice;
		int found = 0;

		if(scanf(" %2d", &choice) == 1){
			LIST_FOREACH(np, &head,entries){
				if(np->topicName == NULL) continue;
				if (np->number == choice){
					local_port = np->localport;
					fprintf(stderr,"localport\n");
					ip_address = np->ip;
					fprintf(stderr,"ip_address\n");
					found = 1;
					break;
					}
				}
		}				
		if(0 == found) {
			printf("wrong input\n");
			exit(1);
		}
		int c;
		while ((c = getchar()) != '\n' && c != EOF);

	/* Set up the address of the server to be contacted. */
	memset((char *) &chat_serv_addr, 0, sizeof(chat_serv_addr));
	chat_serv_addr.sin_family			= AF_INET;
	//expecting a string
	//local_port= ntohl(cli_addr.sin_addr.s_addr);
	chat_serv_addr.sin_addr.s_addr	= htonl(ip_address);
	chat_serv_addr.sin_port			= htons(local_port);

	/* Create a socket (an endpoint for communication). */
	if ((newsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("client: can't open stream socket");
		exit(1);
	}
	//first make a connection to the directory server, send a request for the names, print out those names.
	//what we do is based on the number of arguments that the user passes in 
	// we need the chat_serv_addr , it's sin_family...., ip addr and port

	fprintf(stderr,"server port %d\n",ntohs(chat_serv_addr.sin_port));

	/* Connect to the server. */
	if (connect(newsockfd, (struct sockaddr *) &chat_serv_addr, sizeof(chat_serv_addr)) < 0) {
		perror("client: can't connect to server");
		exit(1);
	}


	//now connect to the server we got
	fprintf(stderr,"messages must be at most 74 characters long or will be truncated by the program.\n");
	fprintf(stderr,"Enter a name to join\n");




	//**************why this */
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
					if(scanf(" %80[^\n]", s) == 1){
						snprintf (message,MAX,"m%s",s);
					}
					else{
						printf("Error reading or parsing user input\n");
					}
				if(s[75]!='\0'){
					printf("Input too long. Will be truncated. You may send another message.\n");
				}
					write(newsockfd, message, MAX);
				}
				else{	
					if(1 == (scanf(" %80[^\t\n]", s)) == 1 ){

					if(s[8]!='\0'){
						
						printf("Name is too long. Try again. 8 characters only\n");
						// printf("Name: ");
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
				}
				//clear any extra input after a tab
				int c;
				while ((c = getchar()) != '\n' && c != EOF);
			


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
			
			memset(s, '\0', MAX);
		}
	}
	//close(sockfd);
	}
}
