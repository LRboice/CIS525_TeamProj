#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"

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
  size_t nwritten;


	
  if (argc == 1) { //handles initial call to directory
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
	  serv_addr.sin_family			= AF_INET;
	  serv_addr.sin_addr.s_addr	= inet_addr(DIR_HOST_ADDR);
	  serv_addr.sin_port			= htons(DIR_TCP_PORT);
     
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		  perror("client: can't open stream socket");
		  exit(1);
	  }

	  /* Connect to the directory. */
	  if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		  perror("client: can't connect to directory");
		  exit(1);
	  }
    snprintf(holder, MAX, "2");
    write(sockfd, holder, MAX); 
    //Note: Will need to write a message to the directory saying that I'm client with 2
    //fprintf(stdout, "Before nread if branch\n");
    if ((nread = read(sockfd, s, MAX)) < 0) { 
		  perror("Error reading from directory\n");
      close(sockfd); 
      exit(1);
	  } 
    else if (nread > 0) {
      //handle reading from directory. will need a loop due to how directory puts stuff to client
        //fprintf(stdout, "In read from Directory branch\n");
        while(nread > 0){
            snprintf(holder, MAX, "Read from server: %s\n", s);
            printf("%s", holder);
            nread = read(sockfd, s, MAX);
        }
        close(sockfd);
        exit(0);				
    }
    else {
      fprintf(stdout, "Directory closed\n");
      close(sockfd);
      exit(0);
    }
  } 
  else if (argc == 3) { //handles connection to server
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

	  for(;;) {

		  FD_ZERO(&readset);
		  FD_SET(STDIN_FILENO, &readset);
		  FD_SET(sockfd, &readset);
      FD_ZERO(&writeset);
      if(readyFlag) {
        FD_SET(sockfd, &writeset);
      }
		  if ((n = select(sockfd+1, &readset, &writeset, NULL, NULL)) > 0) 
		  {
			  /* Check whether there's user input to read */
			  if (FD_ISSET(STDIN_FILENO, &readset)) {
				  //fprintf(stdout, "In client read from terminal.\n");
          //if (1 == scanf(" %[^\n]s", s)) {
          // why need a format string? reading not scanning? - sophia
          
          // reading input to be written TO the server
          if ((n = read(0, tooptr, &(to[MAX]) - tooptr)) < 0){ //this line feels *incredibly* wrong. Might not be able to do format string here - Aidan
            if (errno != EWOULDBLOCK) { perror("read error on socket"); }
          }
          else if (n > 0) { // bytes have been read
            tooptr += n; // add those bytes to the buffer
            // if the buffer is full, send the message
            if(tooptr == &(to[MAX])) {
              readyFlag = 1; // ready to write 
              tooptr = to; // reset buffer
            }
          
					  /* Send the user's message to the server */ 
            //handles case of 1st message to register username
            
            if (userFlag == 0) { // if the username is not set
              //fprintf(stdout, "In userFlag 0 send branch\n");
              snprintf(tooptr, MAX, "1%s", to); // add to buffer with 1 at the beginning
              //write(sockfd, holder, MAX);
              userFlag = 1; // username now set
            }
            else { //catches regular message
              
              //fprintf(stdout, "In userFlag 2 send branch\n");
              snprintf(tooptr, MAX, "2%s", to); // add to buffer with 2 at the beginning 
              //write(sockfd, holder, MAX);
            }
            //write(sockfd, holder, MAX); // write the buffer 
				  } 
          else {
					  printf("Error reading or parsing user input\n");
				  }
		    }
			  /* Check whether there's a message from the server to read */
			  if (FD_ISSET(sockfd, &readset)) { 
				  //fprintf(stdout, "In client read from server.\n");
          // reading FROM the server
          if ((nread = read(sockfd, froptr, &(fr[MAX]) - froptr)) < 0) { 
					  fprintf(stdout, "Error reading from server\n"); 
				  } 
          else if (nread > 0) {
            froptr += nread;
            if(froptr == &(fr[MAX])) { // if the buffer is full
              froptr = fr; // reset pointer
              
              if(userFlag == 1 && strncmp(&fr[0], "1", MAX) == 0){
                fprintf(stdout, "Username already taken. Try again!\n");
                //fprintf(stdout, "In userFlag 1 recieve branch\n");
                userFlag = 0;
              }
              else {
                //fprintf(stdout, "In userFlag 2 recieve branch\n");
                userFlag = 2;
                char printer[MAX];
                snprintf(printer, MAX, "Read from server: %s\n", fr);
                printf("%s", printer);
              }
            }
				  }
          else {
            fprintf(stdout, "Server closed\n");
            close(sockfd);
            exit(0);
          }
			  }
        if(FD_ISSET(sockfd, &writeset)) {
          //fprintf(stdout, "In client write to server.\n");
          size_t pending = tooptr - to; // pending bytes to write
          if (pending > 0) {
            nwritten = write(sockfd, to, MAX);
            if(nwritten < 0) {
              if(errno != EWOULDBLOCK) {
                perror("server: write error on socket");
                // exit ?
              }
            }
            else if(nwritten > 0) {
              if(nwritten >= pending) { // everything was written
                tooptr = to; // reset pointer
                memset(to, 0, MAX); // clear buffer
                readyFlag = 0;
              }
              else {
                int remaining = pending - nwritten;
                for(int i = 0; i < remaining; i++) {
                  to[i] = to[nwritten + i]; // move unwritten bytes to the front
                }
                tooptr = to + nwritten; // update pointer
              }
            }
            //readyFlag = 0;
          }
		    }
      }
      close(sockfd);
      exit(0);
    }
  }
  else {
    perror("Error: Invalid number of arguments.");
    exit(1);
  }
}
