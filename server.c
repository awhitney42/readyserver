// server.c
#include <stdio.h>      // printf, perror
#include <stdbool.h>    // true, false
#include <stdlib.h>     // exit, strtol
#include <signal.h>     // sigaction, SIGINT, SIGTERM
#include <errno.h>      // errno, EINTR
#include <netinet/in.h> // sockaddr_in, INADDR_ANY
#include <sys/socket.h> // socket, bind, listen, accept, setsockopt, AF_INET, SOCK_STREAM
#include <sys/types.h>  // htonl, htons, socklen_t
#include <sys/time.h>   // struct timeval
#include <unistd.h>     // read, write, close

#define MAX_MESSAGE_SIZE 256

/* Port 1. The dark magic port. Privileged. */
#define DEFAULT_PORT 1

/* A default unprivileged port as an alternative. */
/* #define DEFAULT_PORT 6464 */

#define BACKLOG 5
#define READ_TIMEOUT_SEC 5

// sig_atomic_t is the correct type for variables written in signal handlers.
static volatile sig_atomic_t running = 1;
static int server_fd = -1;

static void handle_signal(int sig)
{
  (void)sig;
  // Only set the flag here. close() is not async-signal-safe on all platforms.
  // accept() will return EINTR when interrupted by this signal, and the main
  // loop will detect running == 0 and exit cleanly.
  running = 0;
}

int main(int argc, char *argv[])
{
  // Optional port argument: ./server [port]
  // Use strtol instead of atoi to reliably detect non-numeric input.
  int port = DEFAULT_PORT;
  if (argc == 2) {
    char *end;
    long p = strtol(argv[1], &end, 10);
    if (*end != '\0' || p <= 0 || p > 65535) {
      fprintf(stderr, "Invalid port: %s\n", argv[1]);
      return 1;
    }
    port = (int)p;
  }

  // Register signal handlers for graceful shutdown (SIGINT = Ctrl-C, SIGTERM = kill).
  // sa_flags = 0 (no SA_RESTART) ensures accept() is interrupted by the signal,
  // returning EINTR so the main loop can check the running flag and exit.
  struct sigaction sa = { .sa_handler = handle_signal };
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  // https://man7.org/linux/man-pages/man2/socket.2.html
  // Creates an endpoint for communication and returns a file descriptor that refers to that endpoint.
  // SOCK_STREAM defines that this should communicate over TCP.
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  // SO_REUSEADDR allows the server to bind to a port that is still in TIME_WAIT,
  // avoiding "Address already in use" errors after a restart.
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt SO_REUSEADDR");
    close(server_fd);
    return 1;
  }

  // Initialize the server address struct
  struct sockaddr_in server_addr = {
    .sin_family      = AF_INET,
    .sin_addr.s_addr = htonl(INADDR_ANY),
    .sin_port        = htons((unsigned short)port),
  };

  // https://man7.org/linux/man-pages/man2/bind.2.html
  // bind() assigns the address to the socket.
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("bind");
    close(server_fd);
    return 1;
  }

  // https://man7.org/linux/man-pages/man2/listen.2.html
  // listen() marks the socket as passive; BACKLOG is the max length of the pending connection queue.
  if (listen(server_fd, BACKLOG) < 0) {
    perror("listen");
    close(server_fd);
    return 1;
  }

  printf("Listening on port %d...\n", port);

  // NOTE: This server is single-threaded. It handles one client at a time;
  // a second client will block until the current connection closes or times out.
  struct sockaddr_in client_addr = {0};
  socklen_t client_len;

  // https://man7.org/linux/man-pages/man2/accept4.2.html
  // accept() extracts the first connection request on the queue of pending connections.
  while (running) {
    printf("Waiting for a new connection...\n");

    // Reset client_len before each accept() — accept() writes the actual address
    // size back into it, so it must be re-initialized each iteration.
    client_len = sizeof(client_addr);

    int conn_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (conn_fd < 0) {
      if (!running) break;       // signal received, exit cleanly
      if (errno == EINTR) continue; // interrupted for another reason, retry
      perror("accept");
      continue;
    }
    printf("New connection accepted.\n");

    // SO_RCVTIMEO sets a timeout on blocking recv/read calls so a slow or idle
    // client cannot block the server indefinitely.
    struct timeval timeout = { .tv_sec = READ_TIMEOUT_SEC, .tv_usec = 0 };
    if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
      perror("setsockopt SO_RCVTIMEO");
      close(conn_fd);
      continue;
    }

    char buffer[MAX_MESSAGE_SIZE];
    int b_read;

    while (true) {
      // https://man7.org/linux/man-pages/man2/read.2.html
      // read() attempts to read up to MAX_MESSAGE_SIZE-1 bytes, leaving room for null terminator.
      b_read = read(conn_fd, buffer, sizeof(buffer) - 1);
      if (b_read <= 0) {
        break;
      }
      buffer[b_read] = '\0';  // null-terminate before printing
      printf("%s", buffer);

      // https://man7.org/linux/man-pages/man2/write.2.html
      // write() sends the READY response back to the client.
      const char message[] = "READY.\n";
      if (write(conn_fd, message, sizeof(message) - 1) < 0) {
        perror("write");
        break;
      }
    }

    // https://man7.org/linux/man-pages/man2/close.2.html
    // close() releases the connection file descriptor.
    close(conn_fd);
  }

  close(server_fd);
  printf("\nShutting down.\n");
  return 0;
}