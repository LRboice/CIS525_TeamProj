#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h> 
/*
  NOTE: If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  TODO: have the client have different functionality based on different argc input
*/
int main(int argc, char **argv)
{
	char s[MAX] = {'\0'}; 
	fd_set			readset; 
	int				sockfd;
	struct sockaddr_in serv_addr;
	int				nread;	/* number of characters */
  int       userFlag = 0; //checks if username has been created
  char holder[MAX] = {'\0', '\0'}; //used for concatinating strings with.
  char        argvValOne[MAX];
  int         argvValTwo;
  
  /************************************************************/
  /*** Initialize Client SSL state (from Linux Socket Programming chapter 16)   ***/
  /************************************************************/
  /*const SSL_METHOD *method = TLS_client_method();   /* Create new client-method   

  if(method == NULL) {
    ERR_print_errors_fp(stderr);
    exit(2);
  } */
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());  // Fixed uninitialized ctx
  if (ctx == NULL) {
      ERR_print_errors_fp(stderr);  // Added error handling for SSL_CTX creation
      exit(2);
  }
    
    SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5"); // Set cipher list 
    SSL_load_error_strings();        /* Load/register error msg */
  
  // if return values of the API are null or zero display error message and exit

  
	
	if (argc == 1) { //handles initial call to directory

    /************************************************************/
    /*** Create and connect client's socket to SSL server     ***/
    /************************************************************/

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
	  serv_addr.sin_family			= AF_INET;
	  serv_addr.sin_addr.s_addr	= inet_addr(DIR_HOST_ADDR);
	  serv_addr.sin_port			= htons(DIR_TCP_PORT); 

    /* create socket */
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		  perror("client: can't open stream socket");
		  exit(1);
	  }

	  /* Connect to the directory. */
	  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		  perror("client: can't connect to directory");
		  exit(1);
	  }

    /************************************************************/
    /*** Establish SSL protocol and create encryption link    ***/
    /************************************************************/
    SSL *ssl = SSL_new(ctx); /* create new SSL connection state */
    BIO *bio = BIO_new_socket(sockfd, BIO_NOCLOSE); /*create a new BIO with no close tag set*/
    if(bio == NULL){
      perror("client: BIO_new_socket failed"); 
      exit(1);
    }
    BIO_set_flags(bio, BIO_SOCK_NONBLOCK); /*make it non blocking */
    SSL_set_bio(ssl, bio, bio);/*set bio for ssl con*/ 
    
    if (SSL_connect(ssl) <= 0 ) {     /* perform the connection */
      ERR_print_errors_fp(stderr);        /* report any errors */
      exit(1); 
    }


    /************************************************************/
    /*** Checking certificates                                ***/
    /************************************************************/
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL) {
      char actual[MAX];
      X509_NAME *subject = X509_get_subject_name(cert);
      X509_NAME_get_text_by_NID(subject, NID_commonName, actual, sizeof(actual));
      const char *expected = "Directory Server"; 
      if (strcmp(actual, expected) != 0) {
          fprintf(stderr, "Certificate entity mismatch: expected '%s', got '%s'\n", expected, actual);
          X509_free(cert);
          SSL_free(ssl);  
          close(sockfd);
          exit(1);
      }
    } else {
      perror("No certificates.\n");
      SSL_free(ssl); 
      close(sockfd);
      exit(1);
    }


    snprintf(holder, MAX, "2");
    //write(sockfd, holder, MAX); 
    if(SSL_write(ssl, holder, MAX) < 0)   /* encrypt/send */
    {
      ERR_print_errors_fp(stderr);
    }

    //Note: Will need to write a message to the directory saying that I'm client with 2
    //fprintf(stdout, "Before nread if branch\n");


    if ((nread = SSL_read(ssl, s, MAX)) < 0) { 
		  perror("Error reading from directory\n");
      close(sockfd); 
      exit(1);
	  } 
    else if (nread > 0) {
      //handle reading from directory. will need a loop due to how directory puts stuff to client
        //fprintf(stdout, "In read from Directory branch\n");
        while(nread > 0){
            snprintf(holder, MAX, "Read from server: %.90s\n", s);
            fprintf(stdout, "%s", holder);
            nread = SSL_read(ssl, s, MAX);
        }
        close(sockfd);
        exit(0);				
    }
    else {
      fprintf(stdout, "Directory closed\n");
      close(sockfd);
      exit(0);
    }
    SSL_free(ssl);  /*free it all up end of directory server calls*/ 
    close(sockfd); 
    SSL_CTX_free(ctx); 
    exit(0); 
  } 
  else if (argc == 4) { //handles connection to server Usage: <address> <port> <chatroom name>
    if (sscanf(argv[1], "%s", argvValOne) < 0) {
      perror("Unable to parse address.");
      exit(1);
    }
    if (inet_addr(argvValOne) == (in_addr_t)(-1)){
      perror("Address didn't convert correctly.");
      exit(1);
    }
    if (sscanf(argv[2], "%d", &argvValTwo) < 0){
      perror("Unable to parse port.");
      exit(1);
    }
    if (argvValTwo < 40000 || argvValTwo > 65534){
      perror("Port out of bounds.");
      exit(1);
    }
    /* Set up the address of the server to be contacted. */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
	  serv_addr.sin_family			= AF_INET;
	  serv_addr.sin_addr.s_addr	= inet_addr(argvValOne); //NOTE: these are not error checked. Format is address then port. Might need to do conversions on the address too
	  serv_addr.sin_port			= htons(argvValTwo); //*definitely* need to convert address
 

    fprintf(stdout, "Chat functionality:\n"); 
    fprintf(stdout, "Your first message will register your username.\n");
    fprintf(stdout, "Further messages will appear in chat as regular.\n");
    fprintf(stdout, "To log out, simply close the client\n");

	  /* Create a socket (an endpoint for communication). */
	  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		  perror("client: can't open stream socket");
		  exit(1);
	  }

	  /* Connect to the server. */
	  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		  perror("client: can't connect to server");
		  exit(1);
	  }


    /************************************************************/
    /*** Establish SSL protocol and create encryption link    ***/
    /************************************************************/
    SSL *ssl = SSL_new(ctx); /* create new SSL connection state */
    BIO *bio = BIO_new_socket(sockfd, BIO_NOCLOSE); 
    if(bio == NULL){
      perror("client: BIO_new_socket failed"); 
      exit(1); 
    }
    BIO_set_flags(bio, BIO_SOCK_NONBLOCK); 
    SSL_set_bio(ssl, bio, bio); 

    if (SSL_connect(ssl) == -1 ) {     /* perform the connection */
      ERR_print_errors_fp(stderr);        /* report any errors */
    }

    /************************************************************/
    /*** Checking certificates                                ***/
    /************************************************************/
    X509 *cert = SSL_get_peer_certificate(ssl);
    if (cert != NULL && argv[3] != NULL) {
      char actual[MAX];
      X509_NAME *subject = X509_get_subject_name(cert);
      X509_NAME_get_text_by_NID(subject, NID_commonName, actual, sizeof(actual));
      const char *expected = argv[3]; 
      if (strcmp(actual, expected) != 0) {
          fprintf(stderr, "Certificate entity mismatch: expected '%s', got '%s'\n", expected, actual);
          X509_free(cert);
          SSL_free(ssl);
          close(sockfd);
          exit(1);
      }
    } else {
      printf("No certificates.\n");
      SSL_free(ssl); 
      close(sockfd);
      exit(1);
    }

	  for(;;) {

		  FD_ZERO(&readset);
		  FD_SET(STDIN_FILENO, &readset);
		  FD_SET(sockfd, &readset);
		  if (select(sockfd+1, &readset, NULL, NULL, NULL) > 0) 
		  {
			  /* Check whether there's user input to read */
			  if (FD_ISSET(STDIN_FILENO, &readset)) {
				  //fprintf(stdout, "In client read from terminal.\n");
          if (1 == scanf(" %[^\n]s", s)) {
					  /* Send the user's message to the server */ 
            //handles case of 1st message to register username
             if (userFlag == 0) {
              //fprintf(stdout, "In userFlag 0 send branch\n");
              snprintf(holder, MAX-2, "1%.97s", s);
              SSL_write(ssl, holder, MAX);
              userFlag = 1;
            }
            else {
              //catches regular message
              //fprintf(stdout, "In userFlag 2 send branch\n");
              snprintf(holder, MAX, "2%.98s", s);
              SSL_write(ssl, holder, MAX);
            }
				  } 
          else {
					  printf("Error reading or parsing user input\n");
				  }
		    }
			  /* Check whether there's a message from the server to read */
			  if (FD_ISSET(sockfd, &readset)) { 
				  //fprintf(stdout, "In client read from server.\n");
          if ((nread = SSL_read(ssl, s, MAX)) < 0) { 
					  fprintf(stdout, "Error reading from server\n"); 
				  } 
          else if (nread > 0) {
            if(userFlag == 1 && strncmp(&s[0], "1", MAX) == 0){
              fprintf(stdout, "Username already taken. Try again!\n");
              //fprintf(stdout, "In userFlag 1 recieve branch\n");
              userFlag = 0;
            }
            else {
              //fprintf(stdout, "In userFlag 2 recieve branch\n");
              userFlag = 2;
              char printer[MAX];
              snprintf(printer, MAX, "Read from server: %.90s\n", s);
              fprintf(stdout,"%s", printer);
            }
					
				  }
          else {
            fprintf(stdout, "Server closed\n");
            SSL_free(ssl); /*wrap up mem here */ 
            close(sockfd);
            SSL_CTX_free(ctx);
            exit(0);
          }
			  }
		  }
	  } /*all done lets get outta here */
    SSL_free(ssl); 
    close(sockfd);
    SSL_CTX_free(ctx);
    
    exit(0);
  }
  else {
    fprintf(stderr, "Usage: <IP> <port> <chatroom name>\n");
    exit(1);
  }
}



