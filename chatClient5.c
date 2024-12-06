#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"
#include <openssl/ssl.h>

/*
  NOTE: If any wonkiness happens assume it's something related to that, or the fact that you can't hardcode server addresses/ports
  TODO: have the client have different functionality based on different argc input
*/

int main(int argc, char **argv)
{
	char s[MAX] = {'\0'}; 
	fd_set			readset; 
  fd_set      writeset;
	int				sockfd;
	struct sockaddr_in serv_addr;
	int				nread;	/* number of characters */
  int       userFlag = 0; //checks if username has been created
  char holder[MAX] = {'\0', '\0'}; //used for concatinating strings with.
  char        argvValOne[MAX];
  int         argvValTwo;
  char to[MAX], fr[MAX];
  char *tooptr = to, *froptr = fr;
  int readyFlag = 0, n;
  
/************************************************************/
  /*** Initialize Client SSL state (from Linux Socket Programming chapter 16)   ***/
  /************************************************************/
 

  //OpenSSL_add_all_algorithms();       /* Load cryptos, et.al. */ 
  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!MD5"); // allows only high-security ciphers, exlcuding ciphers without authentication and MD5
  SSL_load_error_strings();        /* Load/register error msg */
  const SSL_METHOD *method = TLS_client_method(); /* Create new client-method */
 
  // if return values of the API are null or zero display error message and exit
  if(method == NULL) {
    ERR_print_errors_fp(stderr);
    exit(2);
  }
  /*SSL_CTX *ctx = SSL_CTX_new(method);            // Create new context after init method 
  if(ctx == NULL) {
    ERR_print_errors_fp(stderr);
    exit(2);
  }*/

	
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
    SSL_set_fd(ssl, sockfd);        /* attach the socket descriptor */
    if (SSL_connect(ssl) <= 0 ) {     /* perform the connection */
      ERR_print_errors_fp(stderr);        /* report any errors */
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
      printf("No certificates.\n");
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
        while(nread > 0 && strncmp(&s[0], "1", MAX) != 0){ //might need to do string comparison here //strncmp(&s[0], "1", MAX) == 0 //s[0] != 1
            snprintf(holder, MAX, "Read from directory: %s\n", s);
            printf("%s", holder);
            nread = SSL_read(ssl, s, MAX);
        }
        fprintf(stdout, "Done reading from directory\n");
        close(sockfd);
        exit(0);				
    }
    else {
      fprintf(stdout, "Directory closed\n");
      close(sockfd);
      exit(0);
    }
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
    if (fcntl(sockfd, F_SETFL, O_NONBLOCK) != 0){
      perror("client: couldn't set new client socket to nonblocking");
      exit(1);
    }
 

    /************************************************************/
    /*** Establish SSL protocol and create encryption link    ***/
    /************************************************************/
    SSL *ssl = SSL_new(ctx); /* create new SSL connection state */
    SSL_set_fd(ssl, sockfd);        /* attach the socket descriptor */
    int x;
    if ((x = SSL_connect(ssl)) == -1 ) {     /* perform the connection */
      ERR_print_errors_fp(stderr);        /* report any errors */
    }
    fprintf(stdout, "Return value of SSL_accept: %d\n", x);

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
    
    //fprintf(stdout, "Before for loop, testing rfd/wfd of ssl vs socket.\n");
    //fprintf(stdout, "sockfd: %d. get_rfd: %d. get_wfd: %d.\n", sockfd, SSL_get_rfd(ssl), SSL_get_wfd(ssl));

	  for(;;) {

		  FD_ZERO(&readset);
		  FD_SET(STDIN_FILENO, &readset);
		  FD_SET(sockfd, &readset);
      FD_ZERO(&writeset);
      if(readyFlag) {
        FD_SET(sockfd, &writeset); //note sure if correct here - Aidan
      }
		  if ((n = select(sockfd+1, &readset, &writeset, NULL, NULL)) > 0) 
		  {
        fprintf(stdout, "Top of select loop.\n");
			  /* Check whether there's user input to read */
			  if (FD_ISSET(STDIN_FILENO, &readset)) {
				  //fprintf(stdout, "In client read from terminal.\n");
          if ((n = read(0, " %[^\n]s", &(fr[MAX]) - froptr)) < 0){ //this line feels *incredibly* wrong. Might not be able to do format string here - Aidan
            if (errno != EWOULDBLOCK) { perror("read error on socket"); }
          }
          else if (n == 0) {
					  /* Send the user's message to the server */ 
            //handles case of 1st message to register username
             if (userFlag == 0) {
              fprintf(stdout, "In userFlag 0 send branch\n");
              snprintf(holder, MAX, "1%s", s);
              SSL_write(ssl, holder, MAX);
              userFlag = 1;
            }
            else {
              //catches regular message
              fprintf(stdout, "In userFlag 2 send branch\n");
              snprintf(holder, MAX, "2%s", s);
              SSL_write(ssl, holder, MAX);
            }
				  } 
          else {
					  printf("Error reading or parsing user input\n");
				  }
		    }
			  /* Check whether there's a message from the server to read */
			  if (FD_ISSET(sockfd, &readset)) { 
				  fprintf(stdout, "In client read from server.\n");
          if ((nread = SSL_read(ssl, s, MAX)) < 0) { 
					  fprintf(stdout, "Error reading from server\n"); 
				  } 
          else if (nread > 0) {
            if(userFlag == 1 && strncmp(&s[0], "1", MAX) == 0){
              fprintf(stdout, "Username already taken. Try again!\n");
              fprintf(stdout, "In userFlag 1 recieve branch\n");
              userFlag = 0;
            }
            else if (userFlag == 0){
              fprintf(stdout, "Welcome to the chat program\n");
            }
            else {
              fprintf(stdout, "In userFlag 2 recieve branch\n");
              userFlag = 2;
              char printer[MAX];
              snprintf(printer, MAX, "Read from server: %s\n", s);
              printf("%s", printer);
            }
					
				  }
          else {
            fprintf(stdout, "Server closed\n");
            SSL_free(ssl);
            close(sockfd);
            SSL_CTX_free(ctx);
            exit(0);
          }
			  }
		  }
	  }
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