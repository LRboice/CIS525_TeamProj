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
    char *name;
    LIST_ENTRY(entry) entries;
};

LIST_HEAD(listhead, entry);

// pass the local port address as arguments
int main(int argc, char **argv)
{
	// if not two arguments and not in format, out of bounds port.
	// exit
	int sockfd, newsockfd;
	unsigned int clilen;
	struct sockaddr_in cli_addr, serv_addr;
	char s[MAX] = {'\0'};
	char topic[MAX] = {'\0'};
	int portNumber;
	int nread;
    char message[MAX] ={'\0'};
    int firstperson = 1;

	/* Set up the address of the server to be contacted. */
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
	serv_addr.sin_port = htons(SERV_TCP_PORT);

	/* Create communication endpoint */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("server: can't open stream socket");
		exit(1);
	}
	/* Connect to the server. */

	if (argc != 3)
	{
		printf("Invalid data. Specify topic name and port number\n");
		exit(1);
	}
	

	int result = sscanf(argv[1], " %20s", topic);
	if (result != 1)
	{
		printf(" error reading inputs\n");
		exit(1);
	}
    //printf("%s",topic);
    if (topic[16] !='\0'){
        printf("Topic name too long. Only 16 characters\n");
        exit(1);
    }

    int n;
    char other;
	result = sscanf(argv[2], "%d%1c", &portNumber,&other);
    //printf(stderr,"%d",result);
	if (result != 1 )
	{
		printf(" error reading input\n");
		exit(1);
	}
    
	// check that the port number is in bounds.
   
	if (portNumber < 40000 || portNumber > 65000 )
	{
		printf("invalid port number\n");
		exit(1);
	}
    fprintf(stderr,"Setting up server at %d\n",portNumber);

        if ((newsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: can't open stream socket");
        exit(1);
    }
    

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(newsockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("server: can't set stream socket address reuse option");
		exit(1);
	}
	//fprintf(stderr,"%d\n",portNumber);
    /* Bind socket to local address */
	
	memset((char *)&cli_addr, 0, sizeof(cli_addr));
	cli_addr.sin_family = AF_INET;
	cli_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	cli_addr.sin_port = htons(portNumber);

	if (bind(newsockfd, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
	{
		perror("server: can't bind local address");
		exit(1);
	}


	snprintf(s, MAX, "s%s %d\n", topic, portNumber);
	//fprintf(stderr, "about to send message %s\n",s);
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("client: can't connect to server");
		exit(1);
	}

    //try to connect to the server before

	write(sockfd, s, MAX);
	//printf("here\n");
	if ((nread = read(sockfd, s, MAX)) <= 0)
	{
		printf("Error reading from server\n");
	}
	else
	{
		printf("Read from server: %s\n", s);
		if (strncmp(s, "SUCCESS", 7) != 0)
		{
            printf("Duplicate topic. Pick another one\n");
			exit(1);
		}
		else{
			printf("Server registered\n");
		}
	}



	struct entry  *np, *end,*n2;
    struct listhead head;
    LIST_INIT(&head);
	clilen = sizeof(cli_addr);
	//initialize readset
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(newsockfd, &readset);
    int max_socket = newsockfd + 1;
    end = malloc(sizeof(struct entry));
    end->socketno = -1;
    //set up lastnode 
	LIST_INSERT_HEAD(&head, end, entries);
	listen(newsockfd, 5);
    fprintf(stderr,"listening now\n");
    
    //set up timeout
    struct timeval timeout;
    timeout.tv_sec = 3;  // 5 seconds
    timeout.tv_usec = 0; // 
    



	for (;;) {

		//reset the readset
		fd_set readsetcopy; /* https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/eeee8b1d-6c1d-4465-8782-04eeb9d48320xhtml */
        readsetcopy = readset;
        int n;



		/* Accept a new connection request */
		if ((n = select(max_socket + 1, &readsetcopy, NULL, NULL, NULL)) > 0)
        {
			{
            // fprintf(stderr,"selected\n");
            //  if it is the listening socket, accept connection requ                                                                                                                                                             est.
            if (FD_ISSET(newsockfd, &readsetcopy))
            {
                fprintf(stderr, "connection request received\n");
                clilen = sizeof(cli_addr);
                int clientsockfd = accept(newsockfd, (struct sockaddr *)&cli_addr, &clilen);
                if (clientsockfd < 0)
                {
                    perror("server: accept error\n");
                }
				else{
                if (clientsockfd > max_socket)
                {
                    max_socket = clientsockfd;
                }
                FD_SET(clientsockfd, &readset);
				}
                // fprintf(stderr,"finished adding\n");
                
            } // if the select socket
            // check every other socket
            for (int i = 0; i < max_socket + 1; i++)
            {
                if (i != newsockfd && FD_ISSET(i, &readsetcopy))
                {
                    // fprintf(stderr, "about to read\n");
                    ssize_t nread;
                    if ((nread = read(i, s, MAX)) < 0)
                    {
                        fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
                        // exit(1);
                    }
                    else if (nread == 0)
                    {
                        fprintf(stderr, "person about to leave\n");
                        //  send the reply to all clients  except the one that has been closed
                        FD_CLR(i, &readset);
                        close(i);
                        //
                        int validperson = 0;
                        LIST_FOREACH(np, &head, entries)
                        {
                            if (np->socketno == i)
                            {
                                snprintf(message, MAX, "%s has left\n", np->name);  

                                validperson = 1;
                                break; 
                            }
                        }
                        
                        // only if the person was part of the chat
                        if (validperson == 1)
                        {
                           
                            LIST_REMOVE(np, entries);
                            
                            free(np->name);
                       
                            free(np);
    

                            LIST_FOREACH(np, &head, entries)
                            {
								if(np->socketno == -1) continue;
                             
                                    if (np->socketno == newsockfd || np->socketno == i )
                                        continue;
                                    write(np->socketno, message, MAX);
                                    
                                
                            }
                        }
                    }
                    else
                    {
                        fprintf(stderr,"inside switch statement\n");
                        switch (s[0])
                        {

                        case 'n':
                                int invalid = 0;
                                fprintf(stderr,"next person to join\n");
								LIST_FOREACH(np, &head, entries)
								{
									if(np->name == NULL) continue;
									if (strncmp(np->name, s + 1, MAX) == 0)
									{
										fprintf(stderr,"duplicate name\n");
										snprintf(message, MAX, "%s", "Duplicate name, already taken\n");
										write(i, message, MAX);
										invalid = 1;
										break;
									}
								}
                                if (invalid == 0)
                                {									
                                    fprintf(stderr,"creating struct\n");
                                    n2 = malloc(sizeof(struct entry));
                                    n2->name = (char *)malloc(50 * sizeof(char));
                                    if (n2->name == NULL)
                                    {
                                        fprintf(stderr, "Memory allocation failed\n");
                                        exit(1);
                                    }
									snprintf(n2->name, MAX, "%s", s + 1);
									n2->socketno = i;


								if (firstperson == 1)
									{
										fprintf(stderr,"first person has joined\n");
										snprintf(message, MAX, " you are the first to join\n");
										firstperson = 0;
                                        LIST_INSERT_BEFORE(end,n2,entries);

										write(i, message, MAX);
									} // if firstperson ==1
								else{
                                    snprintf(message, MAX, "%s has joined\n", n2->name);
									LIST_INSERT_BEFORE(end,n2,entries);
                                    
                                    LIST_FOREACH(np, &head, entries)
                                    {
                                        
                                            if (np->socketno == i || np->socketno == -1){
                                                continue;}

                                            write(np->socketno, message, MAX);
                                     
                                    }
									}
								}	
                            break;                            

                        // senda message to everyone
                        case 'm':

                            LIST_FOREACH(np,&head,entries){
                               
                                if(np->socketno == i){
                                 snprintf(message, MAX, "%s : %s", np->name, s + 1);
                                }
                            }
                            


                            LIST_FOREACH(np, &head, entries)
                            {
                             
                                                                 
                                    if (np->socketno == i || np->socketno == -1 ){
                                        continue;}
                                    write(np->socketno, message, MAX);

                            }

                            break;
                        default:
                            snprintf(s, MAX, "Invalid request\n");
                            write(i,s,MAX);
                        }
                    }
                }
            }
        }
		}
	


	}
}
