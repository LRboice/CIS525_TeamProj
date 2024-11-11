#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h>  

struct entry
{
    int socketno;
    char *topicName;
	int localport;
	int ip;

    LIST_ENTRY(entry) entries;
};

LIST_HEAD(listhead, entry);

int main()
{
	int				sockfd, newsockfd;
	unsigned int	clilen;
	struct sockaddr_in cli_addr, serv_addr;
	char				s[MAX];

	//a linked list where we keep all the connected servers
	struct entry *n1, *np,*end;
    struct listhead head;
    LIST_INIT(&head);

	/* Create communication endpoint */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("server: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("server: can't set stream socket address reuse option");
		exit(1);
	}

	/* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port		= htons(SERV_TCP_PORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("server: can't bind local address");
		exit(1);
	}
		end = malloc(sizeof(struct entry));
		LIST_INSERT_HEAD(&head, end, entries);

		listen(sockfd, 5);


	clilen = sizeof(cli_addr);
	//initialize readset
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(sockfd, &readset);
    int max_socket = sockfd + 1;
	printf("listening\n");

	for (;;) {

		//reset the readset
		fd_set readsetcopy; /* https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/eeee8b1d-6c1d-4465-8782-04eeb9d48320xhtml */
        readsetcopy = readset;
        int n;

		//select


		/* Accept a new connection request */
		if ((n = select(max_socket + 1, &readsetcopy, NULL, NULL, NULL)) > 0)
        {
            fprintf(stderr,"selected\n");
            //  if this is in readsetcopy, doing this because of for                                                                                                                                                              loop
            //  if it is the listening socket, accept connection requ                                                                                                                                                             est.
            if (FD_ISSET(sockfd, &readsetcopy))
            {
                fprintf(stderr, "connection request received\n");
                clilen = sizeof(cli_addr);
                newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
                if (newsockfd < 0)
                {
                    perror("server: accept error");
                    continue;
                }
                if (newsockfd > max_socket)
                {
                    max_socket = newsockfd;
                }
                FD_SET(newsockfd, &readset);
				//save this information to the linkedlist				
				n1 = malloc(sizeof(struct entry));

				n1->socketno = newsockfd;
				n1->ip = ntohl(cli_addr.sin_addr.s_addr);
				
				LIST_INSERT_BEFORE(end,n1,entries);
            }
				
		    for (int i = 0; i < max_socket + 1; i++)
            {
		
                if (i != sockfd && FD_ISSET(i, &readsetcopy))
                {
                    fprintf(stderr, "about to read\n");
                    ssize_t nread;
                    if ((nread = read(i, s, MAX)) < 0)
                    {

                        fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
                        // exit(1);
                    }
                    else if (nread == 0)
                    {
                        fprintf(stderr, "server/client about to leave\n");						
                        FD_CLR(i, &readset);
                        close(i);
						LIST_FOREACH(np, &head, entries){
							if(np->socketno == i){
								
								break;
							}
						}
						//make sure this was a server
						if(np != NULL){
						LIST_REMOVE(np,entries);
						free(np->topicName);
						free(np);
						fprintf(stderr,"after removing\n");
						LIST_FOREACH(np, &head, entries){
							fprintf(stderr,"in the list 1\n");
						}
						}
					}
                        
                    else
                    {
                        switch (s[0])
                        {						
						//case new server
                        case 's':
						fprintf(stderr,"incase newserver\n");
						int portNumber;
						char topic[MAX];  
						int invalid = 0;
						struct entry *curserver;

						//parse the message
						if(2 == sscanf(s+1,"%s %d\n",topic,&portNumber)){
							//fprintf(stderr,"scanfreturnedtwo\n");

						LIST_FOREACH(np, &head, entries)
						{
							if(np->socketno == i){
								
								curserver = np;
							}
							//*we have clients here too that don't have a name*/
							if(np->topicName == NULL) continue;
							//if we find a server with the same credentials
							if (strncmp(np->topicName,topic, MAX) == 0 )
							{
								//what about duplicate ports? 
								//the lists are not being updated when someone leaves
								fprintf(stderr,"duplicate %s\n",np->topicName);
								snprintf(s, MAX, "%s", "Duplicate name, already taken\n");
								//destroy the struct and remove from readset
								FD_CLR(i, &readset);
								write(i, s, MAX);
								close(i);
								invalid = 1;
								break;
							}
							


						}
						if (invalid == 0)
							{

								curserver->topicName = (char *)malloc(50 * sizeof(char));
								if (curserver->topicName == NULL)
								{
									fprintf(stderr, "Memory allocation failed\n");
									exit(1);
								}
								snprintf(curserver->topicName, MAX, "%s", topic);

								snprintf(s, MAX, "SUCCESS");
								curserver->localport = portNumber;
								fprintf(stderr,"sent message SUCCESS\n");
								write(i, s, MAX);

																	
							}//if a valid topic name
							else{
								//find the node with this socket number
								LIST_FOREACH(np, &head, entries){
									if(np->socketno ==i){
										break;
									}
								}
								fprintf(stderr,"freeing up memory");
								LIST_REMOVE(np, entries);
								free(np->topicName);
								free(np);
							}//invalid name
						}
						else
						{
							fprintf(stderr,"error parsing information");
							snprintf(s,MAX,"%s","error parsing information");
							write(i,s,MAX);
						}
						break;
						//client connection request
						case 'c':
						struct entry *client;
							
							//go through the list of server's and if their name is not null, then append to the list
							LIST_FOREACH(np, &head, entries)
							{	
								//only those that are servers
								if(np->topicName != NULL ){
								//fprintf(stderr,"%d %s %d\n",np->ip,np->topicName,np->localport);
								snprintf(s,MAX,"%d %s %d\n",np->ip,np->topicName,np->localport);
								 write(i,s,MAX);
								}
								if(np->socketno == i){
									client = np;
								}
								
							}
								
							close(i);
							LIST_REMOVE(client,entries);
							free(client->topicName);
							free(client);
							FD_CLR(i, &readset);

						default :
							snprintf(s,MAX,"invalid request");
							write(i,s,MAX);


						
						}


                        
            
                    }
                }
            }

		}

		


		// fprintf(stderr, "%s:%d Accepted client connection from %s\n", __FILE__, __LINE__, inet_ntoa(cli_addr.sin_addr));

	
		


	}
}
