#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"

/*
  NOTE: If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  Store the IP as a string
*/

struct connection {
  int userSocket;
  int nicknameFlag;
  char nickname[MAX];
  LIST_ENTRY(connection) clients;
};

LIST_HEAD(listhead, connection);

int main(int argc, char **argv)
{
  if (argc != 3){
    perror("server: wrong number of arguments.");
    exit(1);
  }
	int				sockfd, maxfd, size = 0, nread, dirsock;
	unsigned int	clilen;
	struct sockaddr_in cli_addr, serv_addr, dir_addr;
	char				s[MAX];
  char        addrHolder[MAX];
  char        argvValOne[MAX];
  int         argvValTwo;
  struct listhead head;
  
  if (sscanf(argv[1], "%s", argvValOne) < 0) {
    perror("Unable to parse name.");
    exit(1);
  }
  if (sscanf(argv[2], "%d", &argvValTwo) < 0) {
    perror("Unable to parse port.");
    exit(1);
  }
  if (argvValTwo < 40000 || argvValTwo > 65534){
    perror("Port out of bounds.");
    exit(1);
  }
 
  LIST_INIT(&head);
 
  fd_set readset;

  /* Register with directory */
  memset((char *) &dir_addr, 0, sizeof(dir_addr));
	dir_addr.sin_family = AF_INET;
	dir_addr.sin_addr.s_addr = inet_addr(DIR_HOST_ADDR);
	dir_addr.sin_port		= htons(DIR_TCP_PORT);

  if ((dirsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		  perror("server: can't open stream socket");
		  exit(1);
	  }

	  /* Connect to the directory. */
	if (connect(dirsock, (struct sockaddr *) &dir_addr, sizeof(dir_addr)) < 0) {
		  perror("server: can't connect to directory");
		  exit(1);
	}
 
  /*dirlen = sizeof(dir_addr); //can't resuse sockfd
  int newsockfd = accept(dirsock, (struct sockaddr *) &dir_addr, &dirlen);
  if (newsockfd <= 0) {
    perror("server: accept error from directory");
    exit(1);
  }
  //I think the write would naturally go first, but I might be wrong on that account
  
  int readRet;
  if ((readRet = read(newsockfd, s, MAX)) < 0) { 
    fprintf(stderr, "%s:%d Error reading from directory\n", __FILE__, __LINE__);
    exit(1);
  }  //probably going to be something like this*/
  
    fprintf(stdout, "Before write 1 to server.\n");
    fprintf(stdout, "Value of argvValOne: %s.\n", argvValOne);
        //Note: Will need to write a message to the directory saying that I'm server with 1. If you recieve 1 that means your have the same name as another server
    snprintf(s, MAX, "1%d %s", argvValTwo, argvValOne);
    write(dirsock, s, MAX); //might be dirsock here
    fprintf(stdout, "After write 1 to server.\n");
    if ((nread = read(dirsock, s, MAX)) < 0) { 
		  perror("Error reading from directory\n"); 
      exit(1);
	  } 
    else if (nread > 0) {
        //fprintf(stdout, "In userFlag 2 recieve branch\n");
 			  if(strncmp(&s[0], "1", MAX) == 0){
          perror("Error: server name already taken.\n");
          exit(1);                    
        }
        else{ //you should be good to go if you receive anything else
          sscanf(&s[1], "%s", addrHolder);
          //snprintf(addrHolder, MAX, "%s", &s[1]);
          //inet_addr(addrHolder);
          //close(dirsock);
          fprintf(stdout, "Name accepted.\n");
          //fprintf(stdout, "addrHolder value: %s\n", addrHolder);
        }
    }
    else {
      fprintf(stdout, "Directory closed\n");
      close(dirsock);
      exit(0);
    }
  /* End directory stuff */
 fprintf(stdout, "End directory stuff.\n");
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
  
  //this might have to go after the registering with directory because I don't believe server knows it's own IP
	/* Bind socket to local address */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET; //these two lower sons of bitches throw warnings. Not sure if I need to care
	serv_addr.sin_addr.s_addr = inet_addr(addrHolder); //was originally INADDR_ANY, not sure if this what what needed to change or nah or addrHolder (with read changes)
	serv_addr.sin_port		= htons(argvValTwo); //NOTE: This isn't error checked. Might also need to be converted

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("server: can't bind local address");
		exit(1);
	}

	listen(sockfd, 5);
 
  

	for (;;) {
   //fprintf(stdout, "Top of for loop\n");
   FD_ZERO(&readset);
   FD_SET(sockfd, &readset);
   maxfd = sockfd;
   //loop through clients using linked list and add them to the FD_SET
   struct connection* loopStruct = LIST_FIRST(&head); 
   LIST_FOREACH(loopStruct, &head, clients) {
     
     FD_SET(loopStruct->userSocket, &readset);
     if(loopStruct->userSocket > maxfd){
       maxfd = loopStruct->userSocket;
     }
     //fprintf(stdout, "In adding loop. Current is %d\n", loopStruct->userSocket);
   }
   
   

   //fprintf(stdout, "Right before select statement\n");
   //fprintf(stdout, "Readset 1: %u\n", readset);
   
   if (select(maxfd+1, &readset, NULL, NULL, NULL) > 0){
     //fprintf(stdout, "Readset 2: %u\n", readset);
     //fprintf(stdout, "Right after select statement\n"); 
     /* Accept a new connection request */
		  clilen = sizeof(cli_addr);
      if (FD_ISSET(sockfd, &readset)){
        //fprintf(stdout, "In the accept statement for socket.\n")
        //fprintf(stdout, "Readset 3: %u\n", readset);;
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd <= 0) {
			    perror("server: accept error");
          continue;
		    }
        struct connection* newConnection = malloc(1*sizeof(struct connection));
        newConnection->userSocket = newsockfd;
        newConnection->nickname[0] = '\0';
        newConnection->nicknameFlag = 0;
        LIST_INSERT_HEAD(&head, newConnection, clients); //need to know that this won't have nickname set first time
        size++;
        //fprintf(stdout, "Inserted into head. userSocket val: %d\n", newConnection->userSocket);
      }
      struct connection* tempStruct = LIST_FIRST(&head);
      //fprintf(stdout, "Readset 4: %u\n", readset); 
      while(tempStruct != NULL) {
        //fprintf(stdout, "Top of while loop\n");
        char holder[MAX];
        //fprintf(stdout, "Readset 5: %u\n", readset); 
        if(FD_ISSET(tempStruct->userSocket, &readset)){ 
          //fprintf(stdout, "In if fd_isset\n");
          int nameFlag = 1;
          int readRet;
          if ((readRet = read(tempStruct->userSocket, s, MAX)) < 0) { 
			      fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
          }
          else if (readRet == 0) {
            //Catches client logging out
            snprintf(holder, MAX, "%s has logged out.", tempStruct->nickname);
            close(tempStruct->userSocket); 
            LIST_REMOVE(tempStruct, clients);
            free(tempStruct);
            size--;
            struct connection* sendStruct = LIST_FIRST(&head);
            LIST_FOREACH(sendStruct, &head, clients){
              if (sendStruct->nicknameFlag == 1){
                write(sendStruct->userSocket, holder, MAX);
              }
            }
          }
          /* Generate an appropriate reply */
          //fprintf(stdout, "Before switch statement. readRet: %d\n", readRet);
          else if (readRet > 0) { //checks if the client actually said something
            //int holder;
            //fprintf(stdout, "Right before switch statement\n");
		        switch(s[0]) { /* based on the first character of the client's message */
              //logic: Client side chat will apply a '1' on the first message registering the user's name,
              //and a '2' on any further message that isn't a logout.
              case '1': 
                //code to register name
                //fprintf(stdout, "Test statement: Registered name: %s\n", tempStruct->nickname);
                struct connection* nameStruct = LIST_FIRST(&head);
                while (nameStruct != NULL){
                  if (strncmp(nameStruct->nickname, &s[1], MAX) == 0){
                    nameFlag = 0;
                    //nameStruct->nicknameFlag = 0;
                  }
                  nameStruct = LIST_NEXT(nameStruct, clients);
                } 
                if (nameFlag == 1) {
                  snprintf(tempStruct->nickname, MAX, "%s", &s[1]);
                  tempStruct->nicknameFlag = 1;
                  if(size > 1) {
                    snprintf(holder, MAX, "%s has joined the chat.", tempStruct->nickname);
                  }
                  else{
                    snprintf(holder, MAX, "You are the first user in this chat.");
                  }
                }
                else { //if someone else already has name
                  snprintf(holder, MAX, "1"); 
                  write(tempStruct->userSocket, holder, MAX);
                }
                break;
              case '2':
                //code to do regular message stuff
                //fprintf(stdout, "Test statement: Caught regular message.\n");
                snprintf(holder, MAX-1, "%s: %s", tempStruct->nickname, &s[1]); 
                break;
              case '3':
                //code to exit user
                //this gets handled in readRed == 0
                break;
              default: snprintf(holder, MAX, "Invalid request\n");
            }
            //fprintf(stdout, "End if switch statement\n");
            if (nameFlag == 1){
              struct connection* sendStruct = LIST_FIRST(&head);
              LIST_FOREACH(sendStruct, &head, clients){
                if(sendStruct->nicknameFlag == 1){
                  write(sendStruct->userSocket, holder, MAX);
                }
              }
            }
          }
        }
      tempStruct = LIST_NEXT(tempStruct, clients);
      //fprintf(stdout, "End of while loop\n");
      } 
    }	
	//fprintf(stdout, "End of for loop\n");
  }
}