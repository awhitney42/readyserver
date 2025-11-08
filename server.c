// server.c
#include <stdio.h>      // printf
#include <stdbool.h>	// true, false
#include <netinet/in.h> // sockaddr_in, INADDR_ANY
#include <sys/socket.h> // socket, bind, listen, accept, AF_INET, SOCK_STREAM
#include <sys/types.h>  // htonl, htons, socklen_t
#include <unistd.h>     // read, write, close

#define MAX_MESSAGE_SIZE 256
#define LISTEN_PORT 1

int main(void)
{
  // Initialize the details of the server socket
  struct sockaddr_in server_sockaddr_in;

  // Define socket family AF_INET = internetwork: UDP, TCP, etc.
  server_sockaddr_in.sin_family = AF_INET;

  // The htonl() function converts the unsigned integer hostlong from host byte order to network byte order.
  server_sockaddr_in.sin_addr.s_addr = htonl(INADDR_ANY);

  // Defines the port we want to open this socket on
  const int port = LISTEN_PORT;
  // The htons() function converts the unsigned short integer hostshort from host byte order to network byte order.
  server_sockaddr_in.sin_port = htons(port);

  // https://man7.org/linux/man-pages/man2/socket.2.html
  // creates an endpoint for communication and returns a file descriptor that refers to that endpoint
  // SOCK_STREAM defines that this should communicate over TCP
  int socket_file_descriptor = socket(AF_INET, SOCK_STREAM, 0);

  // https://man7.org/linux/man-pages/man2/bind.2.html
  // bind() assigns the address specified by server_sockaddr_in to the socket socket_file_descriptor
  bind(socket_file_descriptor, (struct sockaddr *)&server_sockaddr_in, sizeof(server_sockaddr_in));

  // https://man7.org/linux/man-pages/man2/listen.2.html
  // listen() marks the socket referred to by sockfd as a passive socket
  // the second parameter (5) defines the maximum length to which the queue of pending connections for socket_file_descriptor may grow
  printf("Listening on port %d...\n", port);
  listen(socket_file_descriptor, 5);
  
  struct sockaddr_in client_sockaddr_in;
  socklen_t len = sizeof(client_sockaddr_in);
  int b_read = 0;
  
  // https://man7.org/linux/man-pages/man2/accept4.2.html
  // accept() extracts the first connection request on the queue of pending connections for the listening socket
  // The address info from the client will be stored in client_sockaddr_in
  while(true) {
    printf("Waiting for a new connection...\n");
    char buffer[MAX_MESSAGE_SIZE] = {};
    // https://man7.org/linux/man-pages/man2/accept4.2.html
    // accept() extracts the first connection request on the queue of pending connections for the listening socket
    // The address info from the client will be stored in client_sockaddr_in
    int connection_file_descriptor = accept(socket_file_descriptor, (struct sockaddr *)&client_sockaddr_in, &len);
    if (connection_file_descriptor < 0) {
      perror("accept");
      continue; // Continue waiting for new connections
    }
    printf("New connection accepted.\n");	    
    while (true) { 
      // https://man7.org/linux/man-pages/man2/read.2.html
      // read() attempts to read up to MAX_MESSAGE_SIZE bytes from file descriptor connection_file_descriptor into buffer
      printf("Attempting data read...\n");
      b_read = read(connection_file_descriptor, buffer, sizeof(buffer));
      if (b_read <= 0) {
        break;
      }
      printf("%s", buffer);
    
      char message[9] = "READY.\n\0";
    
      // https://man7.org/linux/man-pages/man2/write.2.html
      // write(fd, buf, count) writes up to count bytes from the buffer starting at buf to the file referred to by the file descriptor fd
      write(connection_file_descriptor, &message, 8);
    }
    // https://man7.org/linux/man-pages/man2/close.2.html
    // close() closes a file descriptor, so that it no longer refers to any file and may be reused
    close(connection_file_descriptor);
  }
  // https://man7.org/linux/man-pages/man2/close.2.html
  // close() closes a file descriptor, so that it no longer refers to any file and may be reused
  close(socket_file_descriptor);

  return 0;
}

