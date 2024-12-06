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
  struct in_addr conIP;
  int conPort;
  char to[MAX], fr[MAX];
  char *tooptr, *froptr;
  int readyFlag;
  LIST_ENTRY(connection) servers;
  SSL *sslState;
  char write[MAX];
  char read[MAX];
  char *writeptr, *readptr;
  int serverListState;
  struct connection* serverListStruct;


};


LIST_HEAD(listhead, connection);


int main(int argc, char **argv)
{
  int       sockfd, maxfd = 0;
  unsigned int  conlen;
  struct sockaddr_in con_addr, dir_addr;
  struct listhead head;
  int n;
 
  int servPosValue = 1;


 


 
  SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
 


 
  SSL_CTX_use_certificate_file(ctx, "directory.crt", SSL_FILETYPE_PEM);
  SSL_CTX_use_PrivateKey_file(ctx, "directory.key", SSL_FILETYPE_PEM);
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
  dir_addr.sin_port   = htons(DIR_TCP_PORT);


  if (bind(sockfd, (struct sockaddr *) &dir_addr, sizeof(dir_addr)) < 0) {
    perror("directory: can't bind local address");
    exit(1);
  }


  listen(sockfd, 5);


  for (;;) {
   FD_ZERO(&readset);
   FD_SET(sockfd, &readset);
   maxfd = sockfd;


   FD_ZERO(&writeset);
   struct connection* loopStruct = LIST_FIRST(&head);
   LIST_FOREACH(loopStruct, &head, servers) {
     
     FD_SET(loopStruct->conSocket, &readset);
     if (loopStruct->readyFlag == 1){
      FD_SET(loopStruct->conSocket, &writeset);
     }
     if(loopStruct->conSocket > maxfd){
       maxfd = loopStruct->conSocket;
     }
   }


   
   
   if ((n = select(maxfd+1, &readset, &writeset, NULL, NULL)) > 0){
   
      conlen = sizeof(con_addr);
      if (FD_ISSET(sockfd, &readset)){
       
        int newsockfd = accept(sockfd, (struct sockaddr *) &con_addr, &conlen);
        if (newsockfd <= 0) {
          perror("directory: accept error");
          continue;
        }
         
          SSL *  ssl = SSL_new(ctx);      
          SSL_set_fd(ssl, newsockfd);
          if ( SSL_accept(ssl) <1 )    {
            ERR_print_errors_fp(stderr);
            continue;
          }
        if (fcntl(newsockfd, F_SETFL, O_NONBLOCK) != 0){
          perror("server: couldn't set new client socket to nonblocking");
          exit(1);
        }
        struct connection* newConnection = malloc(1*sizeof(struct connection));
        newConnection->conSocket = newsockfd;
        newConnection->servName[0] = '\0';
        newConnection->servNameFlag = 0;
        newConnection->readyFlag = 0;
       
        newConnection->conIP = con_addr.sin_addr;        




       /********* set up linkedlist fields*************/
        newConnection->sslState = ssl;
        newConnection->writeptr = newConnection->write;
        newConnection->readptr = newConnection->read;
        newConnection->serverListState = 0;






        LIST_INSERT_HEAD(&head, newConnection, servers);
      }
      struct connection* tempStruct = LIST_FIRST(&head);
      while(tempStruct != NULL) {
       
        if(FD_ISSET(tempStruct->conSocket, &readset)){
         
          int nameFlag = 1;


       
         


          if ((n = SSL_read(tempStruct->sslState, tempStruct->readptr, &(tempStruct->read[MAX]) - tempStruct->readptr)) < 0){
            if(errno!= EWOULDBLOCK){perror("read error on socket\n");}


            fprintf(stderr, "%s:%d Error reading from client\n", __FILE__, __LINE__);
          }
          else if (n == 0) {
           
            close(tempStruct->conSocket);
            LIST_REMOVE(tempStruct, servers);
           
            free(tempStruct);


          }
          /* Generate an appropriate reply */
          else if (n > 0) { //checks if the client actually said something
           
            tempStruct->readptr += n;
            if (&(tempStruct->read[MAX]) == tempStruct->readptr){
              switch(tempStruct->read[0]){
                case '1':
                  int nameFlag = 1;
                  struct connection* nameStruct = LIST_FIRST(&head);
                  while (nameStruct != NULL){
                    if (nameStruct->servNameFlag != 0){
                      if (strncmp(nameStruct->servName, &(tempStruct->read[1]), MAX) == 0){
                        nameFlag = 0;
                      }
                    }
                 
                    nameStruct = LIST_NEXT(nameStruct, servers);
                  }
                  if (nameFlag == 1){
                    sscanf(&(tempStruct->read[1]), "%d %s", &tempStruct->conPort, tempStruct->servName);
                    tempStruct->servNameFlag = servPosValue;
                    servPosValue++;
                    tempStruct->conIP = con_addr.sin_addr;
                    tempStruct->readyFlag = 1;
                    snprintf(tempStruct->write, MAX, "0%s", inet_ntoa(tempStruct->conIP));
                   


               
                  }
                  else{
                    snprintf(tempStruct->write, MAX, "1");
                    tempStruct->readyFlag = 1;
                   
                  }
                  break;
                case '2':
                  tempStruct->serverListState = 1;
                  tempStruct->readyFlag = 1;
                  struct connection* tempLoop = LIST_FIRST(&head);
                  int flag = 1;
                  while (tempLoop != NULL && flag == 1){
                    if (tempLoop->servNameFlag == 1){
                      flag = 0;
                      tempStruct->serverListStruct = tempLoop;
                    }
                    tempLoop = LIST_NEXT(tempLoop, servers);
                  }
                  if (flag == 0){
                    snprintf(tempStruct->write, MAX, "Server #%d", tempStruct->serverListStruct->servNameFlag);
                  }
                  else{
                    snprintf(tempStruct->write, MAX, "No servers are in the directory.");
                  }
                 
                 
               
                     
                 
                      default: snprintf(tempStruct->write, MAX, "Invalid request\n");
                  }
               
             
            }
           
          }
        }
        else if (FD_ISSET(tempStruct->conSocket, &writeset)){
         
          int nwritten;
          if ((nwritten = SSL_write(tempStruct->sslState, tempStruct->writeptr, &(tempStruct->write[MAX]) - tempStruct->writeptr)) < 0) {
            if (errno != EWOULDBLOCK) { perror("write error on socket"); }
          }
          else {
             
                tempStruct->writeptr += nwritten;
                if (&(tempStruct->write[MAX]) == tempStruct->writeptr) {
                  tempStruct->readyFlag = 0;
                  tempStruct->readptr = tempStruct->read;
                  tempStruct->writeptr = tempStruct->write;
                  if (tempStruct->serverListState != 0){
                      switch(tempStruct->serverListState){
                        case 1:
                          snprintf(tempStruct->write, MAX, "Server IP: %s\n", inet_ntoa(tempStruct->serverListStruct->conIP));
                          tempStruct->readyFlag = 1;
                          tempStruct->serverListState = 2;
                          break;
                        case 2:
                          snprintf(tempStruct->write, MAX, "Server Port: %d\n", tempStruct->serverListStruct->conPort);
                          tempStruct->readyFlag = 1;
                          tempStruct->serverListState = 3;
                          break;
                        case 3:
                          snprintf(tempStruct->write, MAX, "Server Name: %s\n", tempStruct->serverListStruct->servName);
                          tempStruct->readyFlag = 1;
                          tempStruct->serverListState = 4;
                          break;
                        case 4:
                          struct connection* servListLoop = LIST_FIRST(&head);
                          int holder = tempStruct->serverListStruct->servNameFlag;
                          while (servListLoop != NULL && holder == tempStruct->serverListStruct->servNameFlag) {
                            if (servListLoop->servNameFlag > tempStruct->serverListStruct->servNameFlag){
                              tempStruct->serverListStruct = servListLoop;
                            }
                            servListLoop = LIST_NEXT(servListLoop, servers);
                          }
                          if (holder == tempStruct->serverListStruct->servNameFlag){
                            //this branch means you've looped through all the servers
                            tempStruct->serverListState = 0;
                            snprintf(tempStruct->write, MAX, "1");
                            tempStruct->readyFlag = 1;
                          }
                          else{
                            snprintf(tempStruct->write, MAX, "Server #%d", tempStruct->serverListStruct->servNameFlag);
                            tempStruct->readyFlag = 1;
                            tempStruct->serverListState = 1;
                          }
                          break;
                          default: fprintf(stdout, "Hit default\n"); break;
                         


                        //probably need a default/error check state
                      }
              }
                }
                else { fprintf(stderr, "%s:%d: wrote %d bytes \n", __FILE__, __LINE__, nwritten); }
              //}
             
          }
        }
     
        tempStruct = LIST_NEXT(tempStruct, servers);
      }






                       
                 






    }
  }
}







