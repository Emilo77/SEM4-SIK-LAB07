/*
Ten program używa poll(), aby równocześnie obsługiwać wielu klientów
bez tworzenia procesów ani wątków.
*/

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>

#include "common.h"
#include "err.h"

/*
 Program uruchamiamy z dwoma parametrami: nazwa serwera i numer jego portu.
 Program spróbuje połączyć się z serwerem, po czym będzie od nas pobierał
 linie tekstu i wysyłał je do serwera.  Wpisanie "exit" kończy pracę.
*/
using namespace std;

#define BUFFER_SIZE 1024
#define CONNECTIONS 2
#define TIMEOUT 0

char buffer[BUFFER_SIZE];

int main(int argc, char *argv[]) {
  if (argc != 5) {
    fatal("Usage: %s <gui host> <gui port> <server host> <server port> \n",
          argv[0]);
  }

  char *gui_host = argv[1];
  uint16_t gui_port = read_port(argv[2]);

  char *server_host = argv[3];
  uint16_t server_port = read_port(argv[4]);

  struct sockaddr_in gui_address = get_address(gui_host, gui_port, UDP);
  struct sockaddr_in server_address =
      get_address(server_host, server_port, TCP);

  //  printf("Connected to %s:%s\n", argv[1], argv[2]);
  //  printf("Connected to %s:%s\n", argv[3], argv[4]);

  memset(buffer, 0, BUFFER_SIZE);
  struct pollfd poll_descriptors[CONNECTIONS];

  /*	 Inicjujemy tablicę z gniazdkami klientów
           poll_descriptors [0] to gniazdko gui
           poll_descriptors [1] to gniazdko serwera */

  for (auto &poll_descriptor : poll_descriptors) {
    poll_descriptor.fd = -1;
    poll_descriptor.events = POLLIN;
    poll_descriptor.revents = 0;
  }
  size_t active_clients = 0;
  size_t total_clients = 0;
  /* Tworzymy gniazdko centrali */

  poll_descriptors[0].fd = open_udp_socket();
  poll_descriptors[1].fd = open_socket();
  connect_socket(poll_descriptors[1].fd, &server_address);

  while (true) {
    for (auto &poll_descriptor : poll_descriptors) {
      poll_descriptor.revents = 0;
    }

    int poll_status = poll(poll_descriptors, CONNECTIONS, 0);
    if (poll_status == -1) {
      PRINT_ERRNO();
    } else if (poll_status > 0) {

      for (int i = 0; i < CONNECTIONS; i++) {
        if (poll_descriptors[i].fd != -1 &&
            (poll_descriptors[i].revents & (POLLIN | POLLERR))) {
          ssize_t received_bytes =
              read(poll_descriptors[i].fd, buffer, BUFFER_SIZE);
          if (received_bytes < 0) {
            fprintf(stderr,
                    "Error when reading message from gui (errno %d,%s)\n",
                    errno, strerror(errno));
            CHECK_ERRNO(close(poll_descriptors[i].fd));
            poll_descriptors[i].fd = -1;
          } else if (received_bytes == 0) {
            fprintf(stderr, "Ending connection with server\n");
            CHECK_ERRNO(close(poll_descriptors[i].fd));
            poll_descriptors[i].fd = -1;
          } else {
            ssize_t send = 0;
            if (i == UDP) {
              cout << " -(UDP)-> CLIENT -(TCP)->\n";
              send_message_to(poll_descriptors[TCP].fd, &server_address, buffer,
                              received_bytes);
            } else {
              cout << " <-(UDP)- CLIENT <-(TCP)-\n";
              send_message_to(poll_descriptors[UDP].fd, &gui_address, buffer,
                              received_bytes);
            }
            if (send < 0) {
              PRINT_ERRNO();
            }
          }
        }
      }

    }
  }
  return 0;
}
