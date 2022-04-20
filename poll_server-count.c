/*
 Ten program używa poll(), aby równocześnie obsługiwać wielu klientów
 bez tworzenia procesów ani wątków.
*/

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>

#include "common.h"
#include "err.h"

#define BUF_SIZE 1024
#define QUEUE_LENGTH 5
#define TIMEOUT 5000
#define CONNECTIONS 20

static bool finish = false;

/* Obsługa sygnału kończenia */
static void catch_int(int sig) {
	finish = true;
	fprintf(stderr,
	        "Signal %d catched. No new connections will be accepted.\n", sig);
}

int main(int argc, char *argv[]) {

	if(argc != 3) {
		fprintf(stderr, "Usage: %s <port> <control_port>\n", argv[0]);
		return 1;
	}

	int port = (int) strtoul(argv[1], NULL, 10);
	int port_control = (int) strtoul(argv[2], NULL, 10);

	char buf[BUF_SIZE];

	install_signal_handler(SIGINT, catch_int, SA_RESTART);

	struct pollfd poll_descriptors[CONNECTIONS];
	bool is_control[CONNECTIONS] = {false};
	/* Inicjujemy tablicę z gniazdkami klientów, poll_descriptors[0] to gniazdko centrali */
	for (int i = 0; i < CONNECTIONS; ++i) {
		poll_descriptors[i].fd = -1;
		poll_descriptors[i].events = POLLIN;
		poll_descriptors[i].revents = 0;
	}
	size_t active_clients = 0;
	/* Tworzymy gniazdko centrali */
	poll_descriptors[0].fd = open_socket();
	poll_descriptors[1].fd = open_socket();

	bind_socket(poll_descriptors[0].fd, port);
	bind_socket(poll_descriptors[1].fd, port_control);

	printf("Listening clients on port %u\n", port);
	printf("Listening control on port %u\n", port_control);

	start_listening(poll_descriptors[0].fd, QUEUE_LENGTH);
	start_listening(poll_descriptors[1].fd, QUEUE_LENGTH);

	do {
		for (int i = 0; i < CONNECTIONS; ++i) {
			poll_descriptors[i].revents = 0;
		}

		/* Po Ctrl-C zamykamy gniazdko centrali */
		if (finish && poll_descriptors[0].fd >= 0) {
			CHECK_ERRNO(close(poll_descriptors[0].fd));
			poll_descriptors[0].fd = -1;
		}

		int poll_status = poll(poll_descriptors, CONNECTIONS, TIMEOUT);
		if (poll_status == -1) {
			PRINT_ERRNO();
		} else if (poll_status > 0) {
			if (!finish) {
				for(int central_id = 0; central_id < 2; central_id++) {
					if(poll_descriptors[central_id].revents & POLLIN) {
						/* Przyjmuję nowe połączenie */
						int client_fd = accept_connection(poll_descriptors[central_id].fd, NULL);

						bool accepted = false;
						for (int i = 2; i < CONNECTIONS; ++i) {
							if (poll_descriptors[i].fd == -1) {
								if(central_id == 0) {
									fprintf(stderr, "Received new connection (%d)\n", i);
								} else {
									fprintf(stderr, "Received new control (%d)\n", i);
								}

								poll_descriptors[i].fd = client_fd;
								poll_descriptors[i].events = POLLIN;
								if(central_id == 0) {
									is_control[i] = false;
									active_clients++;
								} else {
									is_control[i] = true;
								}
								accepted = true;
								break;
							}
						}
						if (!accepted) {
							CHECK_ERRNO(close(client_fd));
							fprintf(stderr, "Too many clients\n");
						}
					}
				}
			}
			for (int i = 2; i < CONNECTIONS; ++i) {
				if (poll_descriptors[i].fd != -1 &&
				    (poll_descriptors[i].revents & (POLLIN | POLLERR))) {
					ssize_t received_bytes = read(poll_descriptors[i].fd, buf,
					                              BUF_SIZE);
					if (received_bytes < 0) {
						fprintf(stderr,
						        "Error when reading message from connection %d (errno %d, %s)\n",
						        i, errno, strerror(errno));
						CHECK_ERRNO(close(poll_descriptors[i].fd));
						poll_descriptors[i].fd = -1;
						if(!is_control[i]) {
							active_clients -= 1;
						}
					} else if (received_bytes == 0) {
						fprintf(stderr, "Ending connection (%d)\n", i);
						CHECK_ERRNO(close(poll_descriptors[i].fd));
						poll_descriptors[i].fd = -1;
						if(!is_control[i]) {
							active_clients -= 1;
						}
					} else {
						if(is_control[i]) {
							printf("%zu\n", active_clients);
						} else {
							printf("(%d) -->%.*s\n", i, (int) received_bytes, buf);
						}
					}
				}
			}
		} else {
			printf("%d milliseconds passed without any events\n", TIMEOUT);
		}
	} while (!finish || active_clients > 0);

	if (poll_descriptors[0].fd >= 0)
		CHECK_ERRNO(close(poll_descriptors[0].fd));
	if (poll_descriptors[1].fd >= 0)
		CHECK_ERRNO(close(poll_descriptors[1].fd));
	exit(EXIT_SUCCESS);
}
