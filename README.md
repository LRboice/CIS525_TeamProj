# CIS525_TeamProj
Team project for cis525

1. Copy the files to your own directory.

2. Modify inet.h to reflect the host you are currently logged into.
   Also, modify the port numbers to be used to reduce the likelihood
   of conflicting with another server. 

3. Compile the source code using the command: make

4. Start the directory server in teh background or in another window: ./directoryServer5 [Optional '&' for background]

5. Start the server in the background or in another withdow: ./chatServer5 [Topic] [Port number] [Optional '&' for background] 

6. Start the client with the desired server arguments: ./chatClient5 [Server IP] [Server port] [Topic] 

7. Remember to kill the server before logging off.


~

Code overview: chatClient5
Our group began with the chatClient2 version from Aidan's Assignment 4. We implemented changes to the argc for the commandline 
execution to allow for the user to select a desired chatroom from the list of rooms registered in the directory. Additionally, we 
implemented SSL encryption with TLS 1.3 to secure the server connection with verifiable certificates, as well as non-blocking IO
for precise communications with the chatServer5 instance that the user connects to.


Code overview: chatServer5
Our group began with the chatServer2 version from Aidan's Assignment 4. Like the client, we implemented SSL encryption with TLS1.3 which requires the 
chat server to connect and register with the directory server to obtain the valid certification. Once the server is registered with the directory,
it then persistently listens for new client connections and implements nonblocking IO to allow for clients to send messages. This is done using flags to denote
the state of that connections socket (read/write). It will also keep track of the usernames between clients using a flag system 
to denote the position for that client connection.

Code overview: directoryServer5
Our group began with the directoryServer2 from Aidan's Assignment 4. Again, we implemented SSL encrpytion with TLS, it is thus initializing SSL
with its own directory creds. Then, using its own credentials, checks that the connecting server is valid and that its CA's match. From there, it registers the 
information from that connecting server and saves the name, ip, and SSL creds to it's directory. When a client connection occurs, the directory server returns
a list of the currently active servers, their ips, and topic/server names so that the user may connect to them after they are displayed in the client. 
The communications are nonblocking as well, with the same flag system and read/write variables being declared. 