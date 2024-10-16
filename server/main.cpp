#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "defines.h"
#include "hexdump.h"
#include "pollfd_set.h"

#define SERVER_BACKLOG 10
#define INPUT_BUFFER_SIZE 1024

LOCAL bool running;
LOCAL i32 server_socket;
LOCAL pollfd_set_t server_pfds;

void *get_in_addr(struct sockaddr *addr)
{
    if (addr->sa_family == AF_INET) {
        return &((struct sockaddr_in *)addr)->sin_addr;
    }

    return &((struct sockaddr_in6 *)addr)->sin6_addr;
}

void signal_handler(i32 sig)
{
    UNUSED(sig);
    running = false;
}

void handle_new_connection_request_event(void)
{
    struct sockaddr_storage client_addr;
    u32 client_addr_len = sizeof(client_addr);

    i32 client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        fprintf(stderr, "accept error: %s\n", strerror(errno));
        return;
    }

    char client_ip[INET6_ADDRSTRLEN] = {0};
    inet_ntop(client_addr.ss_family,
              get_in_addr((struct sockaddr *)&client_addr),
              client_ip,
              INET6_ADDRSTRLEN);

    u16 port = client_addr.ss_family == AF_INET ?
               ((struct sockaddr_in *)&client_addr)->sin_port :
               ((struct sockaddr_in6 *)&client_addr)->sin6_port;

    printf("new connection from %s:%hu\n", client_ip, port);

    pollfd_set_add(&server_pfds, client_socket);
}

void handle_client_event(i32 client_socket)
{
    u8 buffer[INPUT_BUFFER_SIZE] = {0};
    i64 bytes_read = recv(client_socket, buffer, INPUT_BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            printf("client with socketfd = %d performed orderly shutdown\n", client_socket);
            pollfd_set_remove(&server_pfds, client_socket);
            close(client_socket);
        } else {
            fprintf(stderr, "recv error: %s\n", strerror(errno));
        }
        return;
    }

#if 0
    printf("read %lld bytes of data:\n", bytes_read);
    hexdump(stdout, buffer, bytes_read, HEXDUMP_FLAG_CANONICAL);
#endif
}

LOCAL char *shift(int *argc, char ***argv)
{
    assert(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

LOCAL void usage(FILE *stream, const char *const program)
{
    fprintf(stream, "usage: %s -p <port> [-h]\n", program);
}

int main(int argc, char **argv)
{
    const char *const program = shift(&argc, &argv);
    const char *port_as_cstr = NULL;

    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "-p") == 0) {
            if (argc == 0) {
                fprintf(stderr, "error: missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            port_as_cstr = shift(&argc, &argv);
        } else if (strcmp(flag, "-h") == 0) {
            usage(stdout, program);
            exit(EXIT_SUCCESS);
        } else {
            fprintf(stderr, "error: unknown flag `%s`\n", flag);
            usage(stderr, program);
            exit(EXIT_FAILURE);
        }
    }

    if (port_as_cstr == NULL) {
        fprintf(stderr, "error: port argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    }

    char hostname[256] = {0};
    gethostname(hostname, 256);
    printf("starting the game server on host `%s`\n", hostname);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    i32 status = getaddrinfo(NULL, port_as_cstr, &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_socket == -1) {
            continue;
        }

        const char *ip_version = rp->ai_family == AF_INET  ? "IPv4" :
                                 rp->ai_family == AF_INET6 ? "IPv6" : "UNKNOWN";
        const char *socktype_str = rp->ai_socktype == SOCK_STREAM ? "TCP" :
                                   rp->ai_socktype == SOCK_DGRAM  ? "UDP" : "UNKNOWN";
        fprintf(stderr, "successfully created an %s %s socket(fd=%d)\n", ip_version, socktype_str, server_socket);

        PERSIST i32 yes = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(i32)) == -1) {
            fprintf(stderr, "setsockopt reuse address error: %s\n", strerror(errno));
        }

        if (bind(server_socket, rp->ai_addr, rp->ai_addrlen) == 0) {
            fprintf(stderr, "successfully bound socket(fd=%d)\n", server_socket);
            break;
        } else {
            fprintf(stderr, "failed to bind socket with fd=%d\n", server_socket);
            close(server_socket);
        }
    }

    if (rp == NULL) {
        fprintf(stderr, "could not bind to port %s\n", port_as_cstr);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SERVER_BACKLOG) == -1) {
        fprintf(stderr, "failed to start listening: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        char ip_buffer[INET6_ADDRSTRLEN] = {0};
        inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), ip_buffer, INET6_ADDRSTRLEN);
        fprintf(stderr, "socket listening on %s port %s\n", ip_buffer, port_as_cstr);
    }

    freeaddrinfo(result);

    pollfd_set_init(5, &server_pfds);
    pollfd_set_add(&server_pfds, server_socket);

    struct sigaction sa = {};
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    running = true;

    while (running) {
        i32 num_events = poll(server_pfds.fds, server_pfds.count, POLL_INFINITE_TIMEOUT);

        if (num_events == -1) {
            if (errno == EINTR) {
                fprintf(stderr, "interrupted 'poll' system call\n");
                break;
            }
            fprintf(stderr, "poll error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (u32 i = 0; i < server_pfds.count; i++) {
            if (server_pfds.fds[i].revents & POLLIN) {
                if (server_pfds.fds[i].fd == server_socket) {
                    handle_new_connection_request_event();
                    break;
                } else {
                    handle_client_event(server_pfds.fds[i].fd);
                }
            }
        }
    }

    printf("server shutting down\n");

    pollfd_set_shutdown(&server_pfds);

    if (close(server_socket) == -1) {
        fprintf(stderr, "error while closing the socket: %s\n", strerror(errno));
    }

    return EXIT_SUCCESS;
}
