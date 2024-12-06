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

/*
  NOTE: If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  Store the IP as a string
*/

struct connection {
  int userSocket;
  int nicknameFlag;
  int readyFlag; //1 is ready to read, 0 is not ready
  char nickname[MAX], to[MAX], fr[MAX];
  char *tooptr, *froptr; 
  LIST_ENTRY(connection) clients;
  SSL* cliSSL;
};

LIST_HEAD(listhead, connection);

int main(int argc, char **argv)
{
  if (argc != 3){
    perror("server: wrong number of arguments.");
    exit(1);
  }
	int				sockfd, maxfd, size = 0, nread, dirsock, n, firstFlag = 0;
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
  fd_set writeset;

  /* Initializing SSL Stuff. NOTE: this doesn't do error checking yet 
  Copied from the Linux socket programming chapter 16 */
  //might need an include statement up top - Aidan
  //SSL_library_init(); I'm pretty sure this is automatically done - Aidan
 
  
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  //SSL_METHOD *method = SSLv23_client_method();
  SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);



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
  //the connection with the directory has to not hang, the easiest way is to be nonblocking
  /* SSL Stuff Part II. Copied from the Linux socket programming chapter 16
  I don't think I need to change how the socket was generated previously but I could be wrong*/
  SSL *ssl = SSL_new(ctx);
  SSL_set_fd(ssl, dirsock);
  if (SSL_connect(ssl) == -1)
    ERR_print_errors_fp(stderr);
  
  //fprintf(stdout, "Before write 1 to server.\n");
  //fprintf(stdout, "Value of argvValOne: %s.\n", argvValOne);
      //Note: Will need to write a message to the directory saying that I'm server with 1. If you recieve 1 that means your have the same name as another server

      //come back to this and make it nonblocking
  /*int isDone = 1;
  while (isDone){

  }*/ //NOTE: Ask Eugene if this should be blocking. 
  snprintf(s, MAX, "1%d %s", argvValTwo, argvValOne);
  SSL_write(ssl, s, MAX); //might be dirsock here
  //fprintf(stdout, "After write 1 to server.\n");
  if ((nread = SSL_read(ssl, s, MAX)) < 0) { 
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
  //fprintf(stdout, "End directory stuff.\n");

  /* Setting up SSL server stuff. Copied from Copied from the Linux socket programming chapter 16*/

  //method = SSLv23_server_method(); //I shouldn't need to copy anything else over bc it was loaded earlier - Aidan
  method = TLS_server_method();
  ctx = SSL_CTX_new(method);
  //creating a switch statement based on what server name is to use proper key/cert files
  //NOTE: this may need to go above the directory check statement
  //switch(argvValOne) 
  //{ //NOTE: double check these values are correct with underscores and such. Lucas can't remember
  //the correct values and I don't know where to check - Aidan
  if (strncmp("ksufootball", argvValOne, MAX) == 0)
  {
    SSL_CTX_use_certificate_file(ctx, "ksufootball.crt", SSL_FILETYPE_PEM); //not 100% on this, specifically
    SSL_CTX_use_PrivateKey_file(ctx, "ksufootball.key", SSL_FILETYPE_PEM); //the FILETYPE_PEM - Aidan
    if (!SSL_CTX_check_private_key(ctx))
      fprintf(stderr, "Key & certificate don't match.");
  } 
  else if (strncmp("ksucis", argvValOne, MAX) == 0)
  {
    SSL_CTX_use_certificate_file(ctx, "ksucis.crt", SSL_FILETYPE_PEM); //not 100% on this, specifically
    SSL_CTX_use_PrivateKey_file(ctx, "ksucis.key", SSL_FILETYPE_PEM); //the FILETYPE_PEM - Aidan
    if (!SSL_CTX_check_private_key(ctx)) //double check return value
      fprintf(stderr, "Key & certificate don't match.");
  }
  else
  {
    fprintf(stderr, "ERROR: you used a non-approved server name. Exiting program");
    exit(0);
  }
  //}

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
 
  //SSL_set_accept_state(ssl); //god I hope this fixes is - Aidan

	for (;;) {
   //fprintf(stdout, "Top of for loop\n");
   FD_ZERO(&readset);
   FD_SET(sockfd, &readset);
   maxfd = sockfd;

   FD_ZERO(&writeset);
   //loop through clients using linked list and add them to the FD_SET
   struct connection* loopStruct = LIST_FIRST(&head); 
   LIST_FOREACH(loopStruct, &head, clients) {
     
     FD_SET(loopStruct->userSocket, &readset);
     if (loopStruct->readyFlag == 1) {
      FD_SET(loopStruct->userSocket, &writeset);
     }
     if(loopStruct->userSocket > maxfd){
       maxfd = loopStruct->userSocket;
     }
     fprintf(stdout, "In adding loop. Current is %d\n", loopStruct->userSocket);
   }
   
   

   //fprintf(stdout, "Right before select statement\n");
   //fprintf(stdout, "Readset 1: %u\n", readset);
   
   if (n = select(maxfd+1, &readset, &writeset, NULL, NULL) > 0){
     //fprintf(stdout, "Readset 2: %u\n", readset);
     //fprintf(stdout, "Right after select statement\n"); 
     /* Accept a new connection request */
		  clilen = sizeof(cli_addr);
      if (FD_ISSET(sockfd, &readset)){
        //fprintf(stdout, "In the accept statement for socket.\n");
        //fprintf(stdout, "Readset 3: %u\n", readset);;
        int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd <= 0) {
			    perror("server: accept error");
          continue;
		    }
        if (fcntl(newsockfd, F_SETFL, O_NONBLOCK) != 0) {
          perror("server: couldn't set new client to nonblocking");
          exit(1);
        }
        struct connection* newConnection = malloc(1*sizeof(struct connection));
        newConnection->userSocket = newsockfd;
        newConnection->nickname[0] = '\0';
        newConnection->nicknameFlag = 0;
        newConnection->readyFlag = 0;
        newConnection->tooptr = newConnection->to;
        newConnection->froptr = newConnection->fr;
        LIST_INSERT_HEAD(&head, newConnection, clients); //need to know that this won't have nickname set first time
        size++;
        newConnection->cliSSL = SSL_new(ctx);
        //It doesn't *look* like a need a new ctx or method, but I'm not 100% sure - Aidan
        SSL_set_fd(newConnection->cliSSL, newConnection->userSocket);
        int x;
        if (x = SSL_accept(newConnection->cliSSL) < 0)
          ERR_print_errors_fp(stderr);
        //fprintf(stdout, "Inserted into head. userSocket val: %d\n", newConnection->userSocket);
        //fprintf(stdout, "Return value of SSL_accept: %d\n", x);
        //SSL_write(newConnection->cliSSL, "2Test message from server.", MAX); //this needs to be nonblock
        snprintf(newConnection->to, MAX, "2Please enter your username.");//something like this
        newConnection->readyFlag = 1;
      }
      struct connection* tempStruct = LIST_FIRST(&head);
      //fprintf(stdout, "Readset 4: %u\n", readset); 
      while(tempStruct != NULL) {
        fprintf(stdout, "Top of while loop\n");
        char holder[MAX];
        //fprintf(stdout, "Readset 5: %u\n", readset); 
        if(FD_ISSET(tempStruct->userSocket, &readset)){ 
          fprintf(stdout, "In if fd_isset\n");
          int nameFlag = 1;
          int readRet;
          if ((readRet = SSL_read(tempStruct->cliSSL, tempStruct->froptr, &(tempStruct->fr[MAX]) - tempStruct->froptr)) < 0) { 
			      if (errno != EWOULDBLOCK) { perror("read error on socket"); }
          }
          else if (readRet == 0) {
            //Catches client logging out
            //snprintf(holder, MAX, "%s has logged out.", tempStruct->nickname);
            close(tempStruct->userSocket);
            LIST_REMOVE(tempStruct, clients);
            free(tempStruct);
            //size--;
            //NOTE: need to make the change to fix the client joining issue
            struct connection* sendStruct = LIST_FIRST(&head);
            LIST_FOREACH(sendStruct, &head, clients){
              if (sendStruct->nicknameFlag == 1){
                snprintf(sendStruct->to, MAX, "%s has logged out", tempStruct->nickname);
                sendStruct->readyFlag = 1;
              }
            }
            free(tempStruct);
          }
          /* Generate an appropriate reply */
          //fprintf(stdout, "Before switch statement. readRet: %d\n", readRet);
          else if (readRet > 0) { //checks if the client actually said something
            //int holder;
            tempStruct->froptr += n;
            if (&(tempStruct->fr[MAX]) == tempStruct->froptr) {
              switch(tempStruct->fr[0]) { // based on the first character of the client's message 
              //logic: Client side chat will apply a '1' on the first message registering the user's name,
              //and a '2' on any further message that isn't a logout.
                case '1': 
                //code to register name
                //fprintf(stdout, "Test statement: Registered name: %s\n", tempStruct->nickname);
                int nameFlag = 1;
                  struct connection* nameStruct = LIST_FIRST(&head);
                  //int connectedUsers = 0;
                  while (nameStruct != NULL){ //this could probably be a list foreach, but it currently works and I don't want to mess with it
                    if (nameStruct->nicknameFlag == 1){
                      if (strncmp(nameStruct->nickname, &(tempStruct->fr[1]), MAX) == 0){
                        nameFlag = 0;
                      }
                    }
                  
                    nameStruct = LIST_NEXT(nameStruct, clients);
                  } 
                  if (nameFlag == 1) {
                    snprintf(tempStruct->nickname, MAX-1, "%s", &(tempStruct->fr[1]));
                    tempStruct->nicknameFlag = 1;
                    if (firstFlag == 1){
                      struct connection* writeLoop = LIST_FIRST(&head); 
                      LIST_FOREACH(writeLoop, &head, clients) {
                        if (writeLoop->nicknameFlag == 1){ 
                          snprintf(writeLoop->to, MAX, "%s has joined the chat.", tempStruct->nickname);
                          writeLoop->readyFlag = 1;
                        }
                      }
                      //fprintf(stderr, "%s has joined the chat.\n", tempStruct->nickname);
                    }  //might be extra parenthesis
                else { //if someone else already has name
                    snprintf(tempStruct->to, MAX, "1");
                    tempStruct->readyFlag = 1;
                    //fprintf(stderr, "1 (someone already has nickname)\n");
                  }
                break;
              case '2':
                  //code to do regular message stuff
                  //fprintf(stdout, "Test statement: Caught regular message.\n");
                  struct connection* writeLoop = LIST_FIRST(&head); 
                  LIST_FOREACH(writeLoop, &head, clients) {
                    if (writeLoop->nicknameFlag == 1){ 
                        snprintf(writeLoop->to, MAX, "%s: %s", tempStruct->nickname, &(tempStruct->fr[1]));
                        writeLoop->readyFlag = 1;
                    }
                  }
                  //fprintf(stderr, "%s\n", &(tempStruct->fr[1]));
                  break;
              
              default: break; //might not be the best idea to bbreak here, oh well//snprintf(holder, MAX, "Invalid request\n");
            }
            //fprintf(stdout, "End if switch statement\n");
            /*if (nameFlag == 1){
              struct connection* sendStruct = LIST_FIRST(&head);
              LIST_FOREACH(sendStruct, &head, clients){
                if(sendStruct->nicknameFlag == 1){
                  write(sendStruct->userSocket, holder, MAX);
                }
              }
            }*/
          }
        }
        else if (FD_ISSET(tempStruct->userSocket, &writeset)){ //this might be else if, might not
          //fprintf(stdout, "in FD_ISSET(write).\n");
          int nwritten;
          if ((nwritten = SSL_write(tempStruct->cliSSL, tempStruct->tooptr, &(tempStruct->to[MAX]) - tempStruct->tooptr)) < 0) { //needs to use SSL
            if (errno != EWOULDBLOCK) { perror("write error on socket"); }
          }
          else {
              //fprintf(stdout, "in the write block for sock %d\n", tempStruct->userSocket);
              //fprintf(stdout, "nwritten = %d\n", nwritten);
              tempStruct->tooptr += nwritten;
              if (&(tempStruct->to[MAX]) == tempStruct->tooptr) {
                tempStruct->readyFlag = 0;
                tempStruct->froptr = tempStruct->fr;
                tempStruct->tooptr = tempStruct->to;
                //fprintf(stdout, "This is after it should get reset\n");
              }
              else { fprintf(stderr, "%s:%d: wrote %d bytes \n", __FILE__, __LINE__, nwritten); }
          }
        }
      tempStruct = LIST_NEXT(tempStruct, clients);
      //fprintf(stdout, "End of while loop\n");
      } 
    }	
	//fprintf(stdout, "End of for loop\n");
        
      //tempStruct = LIST_NEXT(tempStruct, clients);
      //fprintf(stdout, "End of while loop\n");
      } 
    }	
	//fprintf(stdout, "End of for loop\n");
  }
}
