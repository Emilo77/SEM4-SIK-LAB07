/*
 Program uruchamiamy z dwoma parametrami: nazwa serwera i numer jego portu.
 Program spróbuje połączyć się z serwerem, po czym będzie od nas pobierał
 linie tekstu i wysyłał je do serwera.  Wpisanie "exit" kończy pracę.
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "err.h"
#include "common.h"

#define BUFFER_SIZE 1024

static const char exit_string[] = "exit";

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fatal("Usage: %s host port \n", argv[0]);
    }

    char *host = argv[1];
    uint16_t port = read_port(argv[2]);
    struct sockaddr_in server_address = get_address(host, port);

    int socket_fd = open_socket();

    connect_socket(socket_fd, &server_address);

    char *server_ip = get_ip(&server_address);
    uint16_t server_port = get_port(&server_address);

    printf("Connected to %s:%d\n", server_ip, server_port);

    char line[BUFFER_SIZE];
    memset(line, 0, BUFFER_SIZE);

    do {
        fgets(line, BUFFER_SIZE - 1, stdin);
        send_message(socket_fd, line, strnlen(line, BUFFER_SIZE), NO_FLAGS);
    } while (strncmp(line, exit_string, sizeof(exit_string) - 1) != 0);

    CHECK_ERRNO(close(socket_fd));

    return 0;
}
