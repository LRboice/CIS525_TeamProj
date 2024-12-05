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
#include <openssl/ssl.h>
#include <openssl/err.h>


/*
  NOTE: this is all copied from the server1. If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  TODO: Handle removing the server name if the server exits or crashes. 
  Note: the server might not need to recieve the IP address, as that's throwing a shitton of errors. Just use ANYADDR in the server and create all of that stuff first maybe?
*/

struct connection {
  int conSocket;
  int servNameFlag;
  char servName[MAX];
  struct in_addr conIP; //I don't think I can keep this as a string. Need to do this as a in_addr
  int conPort;
  char to[MAX], fr[MAX];
  char *tooptr, *froptr;
  int readyFlag;
  LIST_ENTRY(connection) servers;
  SSL *sslState;
  char write[MAX];
  char read[MAX];
  char *writeptr, *readptr;

};
//make the sockets non blocking
//set up the writeset
//if read, read in to np->read
//replace writes with snprintf to the writeptr, and add the linked list to the writeset
//have a seperate for loop where you ckheck if writeset is ready

LIST_HEAD(listhead, connection);

int main(int argc, char **argv)
{
	int				sockfd, maxfd, size = 0;
	unsigned int	conlen;
	struct sockaddr_in con_addr, dir_addr;
	char				s[MAX];
  struct listhead head;
  int n;

  /************************************************************/
  /*** Initialize Server SSL state                          ***/
  /************************************************************/

  // OpenSSL_add_all_algorithms();       /* Load cryptos, et.al. */
 // SSL_load_error_strings();        /* Load/register error msg */

 
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
 

  /************************************************************/
  /*** Load certificate and private key files               ***/
  /************************************************************/
  /* set the local certificate from CertFile */
  SSL_CTX_use_certificate_file(ctx, "directory.crt", SSL_FILETYPE_PEM);
  /* set private key from KeyFile */
  SSL_CTX_use_PrivateKey_file(ctx, "directory.key", SSL_FILETYPE_PEM);
  /* verify private key */
  if ( !SSL_CTX_check_private_key(ctx) )
    fprintf(stderr, "Key & certificate don't match");
 
  LIST_INIT(&head);
 
  fd_set readset;

  /**********  adding writeset */
  fd_set writeset;
  FD_ZERO(&writeset);


  

	/* Create communication endpoint */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("directory: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("directory: can't set stream socket address reuse option");
		exit(1);
	}

	/* Bind socket to local address */
	memset((char *) &dir_addr, 0, sizeof(dir_addr));
	dir_addr.sin_family = AF_INET;
	dir_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dir_addr.sin_port		= htons(DIR_TCP_PORT);

	if (bind(sockfd, (struct sockaddr *) &dir_addr, sizeof(dir_addr)) < 0) {
		perror("directory: can't bind local address");
		exit(1);
	}

	listen(sockfd, 5);

	for (;;) {
   //fprintf(stdout, "Top of for loop\n");
   FD_ZERO(&readset);
   FD_SET(sockfd, &readset);
   maxfd = sockfd;

   FD_ZERO(&writeset);
   //loop through servers using linked list and add them to the FD_SET
   struct connection* loopStruct = LIST_FIRST(&head); 
   struct connection * np;
   LIST_FOREACH(loopStruct, &head, servers) {
     
     FD_SET(loopStruct->conSocket, &readset);
     if (loopStruct->readyFlag == 1){
      FD_SET(loopStruct->conSocket, &writeset);
     }
     if(loopStruct->conSocket > maxfd){
       maxfd = loopStruct->conSocket;
     }
     //fprintf(stdout, "In adding loop. Current is %d\n", loopStruct->conSocket);
   }

   /**************** resset writeset */

        //fd_set writesetcopy; //Writeset is already reset - Aidan
        //writesetcopy = writeset;
  
   

   //fprintf(stdout, "Right before select statement\n");
   //fprintf(stdout, "Readset 1: %u\n", readset);
   
   if ((n = select(maxfd+1, &readset, &writeset, NULL, NULL)) > 0){
   //if (select(maxfd+1, &readset,&writesetcopy, NULL, NULL) > 0){
     //fprintf(stdout, "Readset 2: %u\n", readset);
     //fprintf(stdout, "Right after select statement\n"); 
     /* Accept a new connection request */
		  conlen = sizeof(con_addr);
      if (FD_ISSET(sockfd, &readset)){
        //fprintf(stdout, "In the accept statement for socket.\n")
        //fprintf(stdout, "Readset 3: %u\n", readset);;
        int newsockfd = accept(sockfd, (struct sockaddr *) &con_addr, &conlen);
        if (newsockfd <= 0) {
			    perror("directory: accept error");
          continue;
		    }
          /************************************************************/
          /*** Create SSL session state based on context & SSL_accept */
          /************************************************************/
          SSL *  ssl = SSL_new(ctx);       /* get new SSL state with context */
          SSL_set_fd(ssl, newsockfd); /* associate socket with SSL state */
          if ( SSL_accept(ssl) <1 )    /* do SSL-protocol accept */{
            ERR_print_errors_fp(stderr);
            continue;
          }
        if (fcntl(newsockfd, F_SETFL, O_NONBLOCK) != 0){
          perror("server: couldn't set new client socket to nonblocking");
          exit(1);
        }
        //fprintf(stdout, "Right before add to list\n");
        struct connection* newConnection = malloc(1*sizeof(struct connection));
        newConnection->conSocket = newsockfd;
        newConnection->servName[0] = '\0';
        newConnection->servNameFlag = 0;
        newConnection->readyFlag = 0;
        //newConnection->tooptr = newConnection->to;
        //newConnection->froptr = newConnection->fr;
        newConnection->conIP = con_addr.sin_addr; //note: Might need to copy the memset in line 55 to properly allocate space for this
        //newConnection->conIP.sin_port = con_addr.sin_port; //note: I'm pretty sure this is the port it binds on the directory end, not what clients could use to connect
       


       /********* set up linkedlist fields*************/
        newConnection->sslState = ssl;
        newConnection->writeptr = newConnection->write;
        newConnection->readptr = newConnection->read;



        LIST_INSERT_HEAD(&head, newConnection, servers); //need to know that this won't have servName set first time
        //size++;
        //fprintf(stdout, "Inserted into head. conSocket val: %d\n", newConnection->conSocket);
      }
      struct connection* tempStruct = LIST_FIRST(&head);
      //fprintf(stdout, "Readset 4: %u\n", readset); 
      while(tempStruct != NULL) {
        //fprintf(stdout, "Top of while loop\n");
        char holder[MAX];
        //fprintf(stdout, "Readset 5: %u\n", readset); 
        if(FD_ISSET(tempStruct->conSocket, &readset)){ 
          //fprintf(stdout, "In if fd_isset\n");
          //int readRet;
          int nameFlag = 1;

       
          
          // if ((readRet = read(tempStruct->conSocket, s, MAX)) < 0) 

          /*********************************** read using ssl */
          if ((n = SSL_read(tempStruct->sslState, tempStruct->readptr, &(tempStruct->read[MAX]) - tempStruct->readptr)) < 0){ 
			      if(errno!= EWOULDBLOCK){perror("read error on socket\n");}

            fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
          }
          else if (n == 0) {
            //Catches client/server logging out. With the exception of freeing memory this should be good
            //snprintf(holder, MAX, "%s has logged out.", tempStruct->servName);
            //fprintf(stdout, "Right before remove from list.\n");
            close(tempStruct->conSocket); 
            LIST_REMOVE(tempStruct, servers);
            /*struct connection* writeLoop = LIST_FIRST(&head); 
            LIST_FOREACH(writeLoop, &head, servers) {
            if (writeLoop->servNameFlag == 1){ //I theoretically could just write it to all I think, since that's what I've done on previous assignments
              snprintf(writeLoop->to, MAX, "%s has logged out", tempStruct->servName);
              writeLoop->readyFlag = 1;
              }
            }*/
            free(tempStruct);
            //size--;

          }
          /* Generate an appropriate reply */
          //fprintf(stdout, "Before switch statement. readRet: %d\n", readRet);
          else if (n > 0) { //checks if the client actually said something
            //int holder;
            //fprintf(stdout, "Right before switch statement\n");
            /* sscanf(&s[1], "%d %s", &tempStruct->conPort, tempStruct->servName); //think the & handles making it a pointer.
                fprintf(stdout, "Server name recieved: %s\n", tempStruct->servName);
                fprintf(stdout, "Server port recieved: %d\n", tempStruct->conPort);*/
            tempStruct->readptr += n;
            if (&(tempStruct->read[MAX]) == tempStruct->readptr){
              switch(tempStruct->read[0]){
                case '1':
                  int nameFlag = 1;
                  struct connection* nameStruct = LIST_FIRST(&head);
                  //int connectedUsers = 0;
                  while (nameStruct != NULL){ //this could probably be a list foreach, but it currently works and I don't want to mess with it
                    if (nameStruct->servNameFlag == 1){
                      if (strncmp(nameStruct->servName, &(tempStruct->read[1]), MAX) == 0){
                        nameFlag = 0;
                      }
                    }
                  
                    nameStruct = LIST_NEXT(nameStruct, servers);
                  }
                  if (nameFlag == 1){
                    sscanf(&(tempStruct->read[1]), "%d %s", &tempStruct->conPort, tempStruct->servName); //think the & handles making it a pointer. Might want it to be snprintf
                    tempStruct->servNameFlag = 1;
                    tempStruct->conIP = con_addr.sin_addr;
                    tempStruct->readyFlag = 1;
                    snprintf(tempStruct->to, MAX, "0%s", inet_ntoa(tempStruct->conIP));
                    //snprintf(holder, MAX, "0%s", inet_ntoa(tempStruct->conIP)); //need to figure out something to do with these lines
                    //// write(tempStruct->conSocket, holder, MAX); //99% sure I need a holder variable here
                  SSL_write(tempStruct->sslState, holder, MAX);  //probably shouldn't write here - Aidan

                  /*************** write to the buffer instead ************************/
                 // SSL_write(tempStruct->sslState, holder, MAX); 
                  /*tempStruct->writeptr = tempStruct->write;
                  snprintf(tempStruct->writeptr,MAX,"%s",holder);
                  FD_SET(tempStruct->conSocket,&writeset);*/ //this is the stuff Tatenda did, will need reworked
                  //to fit with git merges - Aidan
                  } 
                  else{
                    snprintf(tempStruct->write, MAX, "1");
                    tempStruct->readyFlag = 1;
                    //snprintf(holder, MAX, "1");  //will need to handle something here
                  //   //write(tempStruct->conSocket, holder, MAX);
                    SSL_write(tempStruct->sslState, holder, MAX); //again, shouldn't write here - Aidan
                  /****************************************** non blocking */
                  /*tempStruct->writeptr = tempStruct->write;
                  snprintf(tempStruct->writeptr,MAX,"%s",holder);
                  FD_SET(tempStruct->conSocket,&writeset);*/ //same git merge jank as before - Aidan
                  }
                  break;
                case '2':
                  fprintf(stdout, "In printing to client.\n");
                  snprintf(holder, MAX, "List of available servers:\n");
                  // write(tempStruct->conSocket, holder, MAX);
                  SSL_write(tempStruct->sslState, holder, MAX); //going to need to rework this to be nonblock
                  //if (nameFlag == 1){ 
                  //fprintf(stdout, "If list is empty: %d\n", LIST_EMPTY(&head));
                  /****************************************** non blocking */
                //SSL_write(tempStruct->sslState, holder, MAX); 
                  /*tempStruct->writeptr = tempStruct->write;
                  snprintf(tempStruct->writeptr,MAX,"%s",holder);
                  tempStruct->writeptr += strlen(holder);*/ //Tatenda's version is correct here, git merge is still jank-Aidan
                  struct connection* sendStruct = LIST_FIRST(&head);
                  //fprintf(stdout, "")
                    LIST_FOREACH(sendStruct, &head, servers){
                      if(sendStruct->servNameFlag == 1){
                        snprintf(holder, MAX, "%s", sendStruct->servName);
                        fprintf(stdout, "First server name: %s\n", sendStruct->servName);
                        // write(tempStruct->conSocket, holder, MAX);
                        /****************************************** non blocking */
                        /*
                        NOTE: This is going to be a headache to make nonblocking
                        best idea is as follows: Have a state variable for which part
                        of the server you are sending and add it to the writeset based
                        off that. The server will have another variable reading which
                        server is currently sending.
                        */ 


                        //SSL_write(tempStruct->sslState, holder, MAX); 
                        snprintf(tempStruct->writeptr,MAX,"%s",holder);
                        tempStruct->writeptr += strlen(holder);


                        snprintf(holder, MAX, "%s", inet_ntoa(sendStruct->conIP));
                        fprintf(stdout, "First server IP: %s\n", inet_ntoa(sendStruct->conIP));
                        // write(tempStruct->conSocket, holder, MAX); //need to send this as a string
                        //SSL_write(tempStruct->sslState, holder, MAX); 

                        snprintf(tempStruct->writeptr,MAX,"%s",holder);
                        tempStruct->writeptr += strlen(holder);



                        snprintf(holder, MAX, "%d", sendStruct->conPort);
                        fprintf(stdout, "First server port: %d\n", sendStruct->conPort);
                        // write(tempStruct->conSocket, holder, MAX);
                       // SSL_write(tempStruct->sslState, holder, MAX); 

                         snprintf(tempStruct->writeptr,MAX,"%s",holder);
                        tempStruct->writeptr += strlen(holder);
                        FD_SET(tempStruct->conSocket,&writeset);


                        fprintf(stdout, "Has printed to client.\n");


                        // sendStruct->conIP, sendStruct->conPort, 
                      }
                  }
                //}
                /*******************   */
                // close(tempStruct->conSocket); 
                // LIST_REMOVE(tempStruct, servers);
                // free(tempStruct); //might need to free the inner struct memory
                // size--;
                // fprintf(stdout, "End of client print.\n");
                // break;
              default: snprintf(holder, MAX, "Invalid request\n");
            }
            //fprintf(stdout, "End if switch statement\n");
            /*if (nameFlag == 1){
              struct connection* sendStruct = LIST_FIRST(&head);
              LIST_FOREACH(sendStruct, &head, servers){
                if(sendStruct->servNameFlag == 1){
                  write(sendStruct->conSocket, holder, MAX);
                }
              }
            }*/ //note: I don't think this is needed, the only time we send stuff is to clients
          }
        }
        else if (FD_ISSET(tempStruct->conSocket, &writeset)){
          int nwritten;
          if ((nwritten = write(tempStruct->conSocket, tempStruct->writeptr, &(tempStruct->write[MAX]) - tempStruct->writeptr)) < 0) {
            if (errno != EWOULDBLOCK) { perror("write error on socket"); }
          }
          else {
              //fprintf(stdout, "in the write block for sock %d\n", tempStruct->userSocket);
              //fprintf(stdout, "nwritten = %d\n", nwritten);
              tempStruct->writeptr += nwritten;
              if (&(tempStruct->write[MAX]) == tempStruct->writeptr) {
                tempStruct->readyFlag = 0;
                tempStruct->read = tempStruct->readptr;
                tempStruct->writeptr = tempStruct->writeptr;
                //fprintf(stdout, "This is after it should get reset\n");
              }
              else { fprintf(stderr, "%s:%d: wrote %d bytes \n", __FILE__, __LINE__, nwritten); }
          }
        }
        tempStruct = LIST_NEXT(tempStruct, servers);
      //fprintf(stdout, "End of while loop\n");
      } 


      /*************************  */
            /*LIST_FOREACH(np, &head, servers){ //this is Tatenda's write and I don't want to look at it - Aidan

                    if(FD_ISSET(np->conSocket, &writesetcopy) && (*(np->write) != '\0'))
                    {
                        fprintf(stderr,"ready to write\n");
                        int nwritten;       
                        //SSL_write(tempStruct->sslState, holder, MAX);            
                        if ((nwritten = SSL_write(np->sslState, np->writeptr,MAX) < 0)) {
                        if (errno != EWOULDBLOCK) { perror("write error on socket"); }
                        }
                        else {
                            np->writeptr += nwritten;  //bytes just written *
                            fprintf(stderr,"just wrote %d bytes\n",nwritten);
                        if (&(np->write[MAX]) == np->writeptr) {
                            np->writeptr = np->write;
                            FD_CLR(np->conSocket,&writeset);
                            memset(np->write, '\0', MAX);

                        }
                        }
                        
                      
                    }
                } */




    }	
	//fprintf(stdout, "End of for loop\n");
  }
}


