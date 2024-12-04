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
#include <openssl/bio.h>
#include <openssl/err.h>


/*
  NOTE: If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  Store the IP as a string
*/

struct connection {
  int userSocket;
  int nicknameFlag;
  char nickname[MAX];
  SSL* cliSSL; //not 100% sure on this generation - Aidan
  BIO* bio; 
  LIST_ENTRY(connection) clients;
};

LIST_HEAD(listhead, connection);

/*******       The error handling and retries require helpers, I cant handle the scrolling anymore    ***********/
/* this one is for writing, has three possible results. 
   They are as such: 
   * >0: num of successfully written bytes (good for truncation fixing if it messes anything up)
      0: retry the call , BIO_should_retry is true 
     -1: Error got thrown, not able to retry. 
 */
int ssl_nonbio_write(SSL *ssl,  char *buf, int len){
  int retMe = SSL_write(ssl, buf, len); 
  if (retMe <= 0){
      if(!BIO_should_retry(SSL_get_wbio(ssl))){
        return -1; //can't retry its false!!!!!!!!!!!!!!1
      }
      else{ //flag for retry attempt, godspeed
        return 0; 
      }
  }
  return retMe; 
}
/* Basically the same as the write helper but
   for read. Same return codes, but the sucessful 
   call returns the number of READ bytes. 
 */
int ssl_nonbio_read(SSL *ssl, char *buf, int len)
{
  int retMe = SSL_read(ssl, buf, len); 
  if(retMe <= 0){
    if(!BIO_should_retry(SSL_get_rbio(ssl))){
      return -1; 
    }
    else{
      return 0; 
    }
  }
  return retMe; 
}

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

  /* Initializing SSL Stuff. NOTE: this doesn't do error checking yet 
  Copied from the Linux socket programming chapter 16 */
  //might need an include statement up top - Aidan
  //SSL_library_init(); I'm pretty sure this is automatically done - Aidan
 
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  //method = SSLv23_client_method(); don't think we need this one yet thats later - lucas
  const SSL_METHOD *method = TLS_server_method(); 
  SSL_CTX *ctx = SSL_CTX_new(method);

  if (!ctx) {
    ERR_print_errors_fp(stderr);
    exit(1);
  }
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

  /* SSL Stuff Part II. Copied from the Linux socket programming chapter 16
  I don't think I need to change how the socket was generated previously but I could be wrong*/
  SSL *ssl = SSL_new(ctx);
  BIO *dirBIO = BIO_new_socket(dirsock, BIO_NOCLOSE); 
  BIO_set_nbio(dirBIO, 1); 
  SSL_set_bio(ssl, dirBIO, dirBIO);

  if (SSL_connect(ssl) <= 0 && !BIO_should_retry(dirBIO)) {
    ERR_print_errors_fp(stderr);
    exit(1);
  }
  //char* cipher_name = SSL_get_cipher(ssl); //I don't think this is needed but it's commented out here just in case - Aidan

  /*char line[1024];
  X509 *x509 = X509_get_subject_name(cert);
  X509_NAME_oneline(x509, line, sizeof[line]); // Convert it 
  printf("Subject: %s\ n", line);
  x509 = X509_get_issuer_name(cert);            // get issuer 
  X509_NAME_oneline(x509, line, sizeof(line));  // convert it 
  printf("Issuer: %s\ n", line);*/
  //I'm not 100% sure how to use this rn, but I'm copying it over for the time being - Aidan
  
    //fprintf(stdout, "Before write 1 to server.\n");
    //fprintf(stdout, "Value of argvValOne: %s.\n", argvValOne);
        //Note: Will need to write a message to the directory saying that I'm server with 1. If you recieve 1 that means your have the same name as another server
    snprintf(s, MAX, "1%d %.1000s", argvValTwo, argvValOne);
    
    if (ssl_nonbio_write(ssl, s, MAX) == -1) {
      perror("Error writing to directory\n");
      exit(1);
    } 
    //fprintf(stdout, "After write 1 to server.\n");
    nread = ssl_nonbio_read(ssl, s, MAX); 
   
    if (nread == -1) {
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
  // had to modify it as it was having context init errors - lucas
  const SSL_METHOD *server_method = TLS_server_method(); 
  SSL_CTX *srv_ctx = SSL_CTX_new(server_method); 
  if (!srv_ctx) {
    ERR_print_errors_fp(stderr);
    exit(1);
  }

  //creating a switch statement based on what server name is to use proper key/cert files
  //NOTE: this may need to go above the directory check statement
  //switch(argvValOne) 
  //{ //NOTE: double check these values are correct with underscores and such. Lucas can't remember
  //the correct values and I don't know where to check - Aidan
  //
  // Names and cert file subjs are corrected. 
  // I am gonna put them in this note tho too.
  // FOR KSU CHATS:
  //    > "ksuFootball..." for this server name
  //    > "ksucis..." for the other server name
  // FOR DIRECTORY SERVER CA:
  //    > "Directory Server" 
  // FOR ksuCA key certs & subj (if needed):  
  //    > "KSU Chat CA"
  //
  // You can check the names of the keys and certs in 
  // the directory containing the rest of the project files. 
  //  - lucas
  if (strncmp("KSU Football", argvValOne, MAX) == 0)
  {
    SSL_CTX_use_certificate_file(srv_ctx, "ksufootball.crt", SSL_FILETYPE_PEM); //not 100% on this, specifically
    SSL_CTX_use_PrivateKey_file(srv_ctx, "ksufootball.key", SSL_FILETYPE_PEM); //the FILETYPE_PEM - Aidan
    if (!SSL_CTX_check_private_key(srv_ctx))
      fprintf(stderr, "Key & certificate don't match.");
  } 
  else if (strncmp("KSU CIS", argvValOne, MAX) == 0)
  {
    SSL_CTX_use_certificate_file(srv_ctx, "ksucis.crt", SSL_FILETYPE_PEM); //not 100% on this, specifically
    SSL_CTX_use_PrivateKey_file(srv_ctx, "ksucis.key", SSL_FILETYPE_PEM); //the FILETYPE_PEM - Aidan
    if (!SSL_CTX_check_private_key(srv_ctx))
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
        /* add new ssl for bio btwn server and client */
        newConnection->cliSSL = SSL_new(srv_ctx);
        BIO *clientBio = BIO_new_socket(newsockfd, BIO_NOCLOSE); 
        BIO_set_nbio(clientBio, 1); 
        //It doesn't *look* like a need a new ctx or method, but I'm not 100% sure - Aidan
        // It's good we just needed ctx. 
        SSL_set_bio(newConnection->cliSSL, clientBio, clientBio);

        if (SSL_accept(newConnection->cliSSL) < 0){
          if (!BIO_should_retry(clientBio)) {
            ERR_print_errors_fp(stderr);
            close(newsockfd);
            free(newConnection);
            continue;
          }
        }
        //fprintf(stdout, "Inserted into head. userSocket val: %d\n", newConnection->userSocket);
        LIST_INSERT_HEAD(&head, newConnection, clients); //need to know that this won't have nickname set first time
        size++; 
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
          int readRet = ssl_nonbio_read(tempStruct->cliSSL, s, MAX);
          if (readRet == -1) { 
            /* cant retry on non blocking since we have an error */  
            snprintf(holder, MAX, "%.990s has logged out.", tempStruct->nickname);
            close(tempStruct->userSocket);
            //SSL_shutdown(tempStruct->cliSSL);
            SSL_free(tempStruct->cliSSL); 
            LIST_REMOVE(tempStruct, clients);
            free(tempStruct);
            size--;
            //NOTE: need to make the change to fix the client joining issue
            //logout here since no retries
            struct connection* sendStruct = LIST_FIRST(&head);
            LIST_FOREACH(sendStruct, &head, clients){
              if (sendStruct->nicknameFlag == 1){
                int wRet = ssl_nonbio_write(sendStruct->cliSSL, holder, MAX);
                if (wRet == -1) {
                  // Non-retryable write error; treat like disconnect
                  close(sendStruct->userSocket);
                  SSL_free(sendStruct->cliSSL);
                  LIST_REMOVE(sendStruct, clients);
                  free(sendStruct);
                  size--;
                }
              }
            }
          }
          else if(readRet == 0){
            //retryable read don't have to do anything. 
            tempStruct = LIST_NEXT(tempStruct, clients);
            continue; 
          }
          /* Generate an appropriate reply */
          //fprintf(stdout, "Before switch statement. readRet: %d\n", readRet);
          else if (readRet > 0) { //checks if the client actually said something
            struct connection *nameStruct; 
            int wRet; 
            //int holder;
            //fprintf(stdout, "Right before switch statement\n");
		        switch(s[0]) { /* based on the first character of the client's message */
              //logic: Client side chat will apply a '1' on the first message registering the user's name,
              //and a '2' on any further message that isn't a logout.
              case '1': 
                //code to register name
                //fprintf(stdout, "Test statement: Registered name: %s\n", tempStruct->nickname);
                nameStruct = LIST_FIRST(&head);
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
                    snprintf(holder, MAX, "%.900s has joined the chat.", tempStruct->nickname);
                  }
                  else{
                    snprintf(holder, MAX, "You are the first user in this chat.");
                  }
                }
                else { //if someone else already has name
                  snprintf(holder, MAX, "1"); 
                  wRet = ssl_nonbio_write(tempStruct->cliSSL, holder, MAX); 
                  if (wRet == -1){
                    close(tempStruct->userSocket); 
                    SSL_free(tempStruct->cliSSL); 
                    LIST_REMOVE(tempStruct, clients); 
                    free(tempStruct); 
                    size--; 
                  }
                } 
                break;
              }
              case '2':
                //code to do regular message stuff
                //fprintf(stdout, "Test statement: Caught regular message.\n");
                snprintf(holder, MAX-1, "%.500s: %.500s", tempStruct->nickname, &s[1]); 
                break;
              case '3':
                //code to exit user
                //this gets handled in readRed == -1
                break;
              default: snprintf(holder, MAX, "Invalid request\n");
            }
            //fprintf(stdout, "End if switch statement\n");
            if (nameFlag == 1){
              struct connection* sendStruct = LIST_FIRST(&head);
              LIST_FOREACH(sendStruct, &head, clients){
                if(sendStruct->nicknameFlag == 1){
                  wRet = ssl_nonbio_write(sendStruct->cliSSL, holder, MAX);
                  if(wRet == -1){
                     //disconnect due to non retriable error
                     close(sendStruct->userSocket); 
                     SSL_free(sendStruct->cliSSL); 
                     LIST_REMOVE(sendStruct, clients);
                     free(sendStruct); 
                     size--; 
                  }
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
