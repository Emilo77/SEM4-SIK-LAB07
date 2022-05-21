#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "err.h"

using namespace std;

#define BUFFER_SIZE     40
#define QUEUE_LENGTH     5

char buffer[BUFFER_SIZE];

void print_message(size_t read_length) {
  cout << "Received: ";
  for(int i = 0; i < read_length; i++) {
    std:: cout << buffer[i];
  }
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fatal("Usage: %s <port>", argv[0]);
  }

  uint16_t port = read_port(argv[1]);

  int socket_fd = open_socket();
  bind_socket(socket_fd, port);

  // switch to listening (passive open)
  start_listening(socket_fd, QUEUE_LENGTH);

  printf("Listening on port %u\n", port);

  for (;;) {
    memset(buffer, 0, BUFFER_SIZE);
    struct sockaddr_in client_address;
    int client_fd = accept_connection(socket_fd, &client_address);
    char *client_ip = inet_ntoa(client_address.sin_addr);
    // We don't need to free this,
    // it is a pointer to a static buffer.

    uint16_t client_port = ntohs(client_address.sin_port);
    printf("Accepted connection from %s:%d\n", client_ip, client_port);

    // Reading needs to be done in a loop, because:
    // 1. the client may send a message that is larger than the buffer
    // 2. a single read() call may not read the entire message, even if it fits in the buffer
    // 3. in general, there is no rule that for each client's write(), there will be a corresponding read()
    size_t read_length = 6;
    char message[1024];
    strcpy(message, "Kot\n");

    do {
      if (read_length > 0) {
        memset(buffer, 0, BUFFER_SIZE); // clean the buffer

        memcpy(buffer, message, strlen(message));
        send_message_to(client_fd, &client_address, buffer, strlen(message));
        read_length = receive_message(client_fd, buffer, BUFFER_SIZE, 0);
        if (read_length < 0) {
          PRINT_ERRNO();
        }
        print_message(read_length);
      }
    } while (read_length > 0);
    printf("Closing connection\n");
    CHECK(close(client_fd));

    if (strncmp(buffer, "exit", 4) == 0) {
      break;
    }
  }

  CHECK(close(socket_fd));

  return 0;
}