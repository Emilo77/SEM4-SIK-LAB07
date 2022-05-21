#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "err.h"

#define BUFFER_SIZE 90000

char buffer[BUFFER_SIZE];

using namespace std;

void print_message(size_t read_length) {
  cout << "Received: ";
  for (int i = 0; i < read_length; i++) {
    std::cout << buffer[i];
  }
}

int bind_socket(uint16_t port) {
  int socket_fd = socket(AF_INET, SOCK_DGRAM, 0); // creating IPv4 UDP socket
  ENSURE(socket_fd > 0);
  // after socket() call; we should close(sock) on any execution path;

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET; // IPv4
  server_address.sin_addr.s_addr =
      htonl(INADDR_ANY); // listening on all interfaces
  server_address.sin_port = htons(port);

  // bind the socket to a concrete address
  CHECK_ERRNO(bind(socket_fd, (struct sockaddr *)&server_address,
                   (socklen_t)sizeof(server_address)));

  return socket_fd;
}

size_t read_message(int socket_fd, struct sockaddr_in *client_address,
                    char *buffer, size_t max_length) {
  socklen_t address_length = (socklen_t)sizeof(*client_address);
  int flags = 0; // we do not request anything special
  errno = 0;
  ssize_t len = recvfrom(socket_fd, buffer, max_length, flags,
                         (struct sockaddr *)client_address, &address_length);
  if (len < 0) {
    PRINT_ERRNO();
  }
  return (size_t)len;
}



int main(int argc, char *argv[]) {
  if (argc != 2) {
    fatal("usage: %s <port>", argv[0]);
  }

  uint16_t port = read_port(argv[1]);
  printf("Listening on port %u\n", port);

  memset(buffer, 0, BUFFER_SIZE);

  int socket_fd = bind_socket(port);
  char message[1024];
  strcpy(message, "Pies\n");

  struct sockaddr_in client_address;
  size_t read_length;

  while (true) {
    memset(buffer, 0, BUFFER_SIZE); // clean the buffer
    read_length = read_message(socket_fd, &client_address, buffer, BUFFER_SIZE);
    print_message(read_length);
    if (read_length < 0) {
      cout << "Nie udało się wczytać wiadomości!\n";
    } else {
      memset(buffer, 0, BUFFER_SIZE); // clean the buffer
      memcpy(buffer, message, strlen(message));
      send_message_to(socket_fd, &client_address, buffer, strlen(message));
    }
  }
  return 0;
}
