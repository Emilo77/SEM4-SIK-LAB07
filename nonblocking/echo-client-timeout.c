/*
 Program uruchamiamy z dwoma parametrami: nazwa serwera i numer jego portu.
 Program spróbuje połączyć się z serwerem, po czym będzie od nas pobierał
 linie tekstu i wysyłał je do serwera. W momencie, gdy wysyłanie się nie 
 powiedzie, próbuje czytać od serwera i wypisywać na wyjście.
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "err.h"
#include "common.h"

#define BUFFER_SIZE 1024

void read_from_socket(int sock) {
  char buffer[BUFFER_SIZE];
  ssize_t len;
  static size_t total_len = 0;
  
  len = read(sock, buffer, sizeof(buffer));
  if (len < 0)
    PRINT_ERRNO();
  if (len == 0) 
    fatal("Ending connection");
  
  total_len += len;
  fprintf(stderr, "Received %zd bytes (%zd in total)\n", len, total_len);
  printf("%.*s\n", (int)len, buffer);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("Usage: %s host port \n", argv[0]);
    }

    char *host = argv[1];
    uint16_t port = read_port(argv[2]);
    struct sockaddr_in server_address = get_address(host, port);

    int socket_fd = open_socket();

    connect_socket(socket_fd, &server_address);

    /* Ustawiamy timeout (0.5 sekundy) dla zapisu do gniazda */
    struct timeval timeout;  
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000;
    CHECK_ERRNO(setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)));

    char *server_ip = get_ip(&server_address);
    uint16_t server_port = get_port(&server_address);

    printf("Connected to %s:%d\n", server_ip, server_port);

    char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);
    ssize_t to_send, send_from, len_sent, total_sent;
   
    total_sent = 0;
    for (;;) {
       if (fgets(line, BUFFER_SIZE - 1, stdin) == NULL)
            break; /* koniec wejścia */
      
        to_send = strlen(line);
        send_from = 0;
        while (to_send > 0) {
            len_sent = write(socket_fd, line + send_from, to_send);

            if (len_sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                len_sent = 0; /* timeout przy próbie zapisu */
      
            if (len_sent < 0)
                PRINT_ERRNO();

            to_send -= len_sent;
            send_from += len_sent;
            total_sent += len_sent;

            fprintf(stderr, "Sent %zd bytes (%zd in total)\n", len_sent, total_sent);

            if (to_send > 0)
                read_from_socket(socket_fd);
        }
    }

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
