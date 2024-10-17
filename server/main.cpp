#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sqlite3.h>

#include "defines.h"
#include "hexdump.h"
#include "pollfd_set.h"

#define SERVER_BACKLOG 10
#define INPUT_BUFFER_SIZE 1024
#define DEFAULT_DATABASE_FILEPATH "db"

LOCAL bool running;
LOCAL i32 server_socket;
LOCAL pollfd_set_t server_pfds;
LOCAL sqlite3 *server_db = NULL;

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

LOCAL bool db_execute_sql(sqlite3 *db, const char *const sql)
{
    assert(db != NULL);

    char *err = NULL;
    i32 rc = sqlite3_exec(db, sql, 0, 0, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "error: failed to execute sql query: %s\n", err);
        sqlite3_free(err);
        return false;
    }

    return true;
}

LOCAL bool db_table_exists(sqlite3 *db, const char *table_name)
{
    assert(db != NULL);

    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT name FROM sqlite_master WHERE type='table' AND name='%s';", table_name);

    sqlite3_stmt *stmt;
    i32 rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "error: failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

LOCAL bool db_verify_tables(sqlite3 *db, const char *database_filepath)
{
    assert(db != NULL);

    printf("verifying `%s` database tables\n", database_filepath);

    if (!db_table_exists(db, "ip_blacklist")) {
        const char *sql_create_ip_blacklist_table =
            "CREATE TABLE ip_blacklist (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    ip_address TEXT NOT NULL UNIQUE,\n"
            "    description TEXT,\n"
            "    created_at DATETIME DEFAULT (DATETIME('now', 'localtime'))\n"
            ");";

        printf("creating 'ip_blacklist' table...");
        if (!db_execute_sql(db, sql_create_ip_blacklist_table)) {
            printf(" ERROR\n");
            return false;
        } else {
            printf(" OK\n");
        }
    }

    printf("database verification completed successfully\n");
    return true;
}

LOCAL bool db_print_query_result(FILE *stream, sqlite3 *db, const char *const sql)
{
    assert(db != NULL);

    sqlite3_stmt *stmt;
    i32 rc;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "error: failed to prepare stetement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    fprintf(stream, "sql query:\n");
    fprintf(stream, "  %s\n", sql);
    fprintf(stream, "results:\n  ");

    i32 cols = sqlite3_column_count(stmt);
    for (i32 i = 0; i < cols; i++) {
        const char *column_name = sqlite3_column_name(stmt, i);
        fprintf(stream, "%s ", column_name);
    }
    fprintf(stream, "\n");

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        fprintf(stream, "  ");
        for (i32 i = 0; i < cols; i++) {
            i32 col_type = sqlite3_column_type(stmt, i);
            switch (col_type) {
            case SQLITE_INTEGER:
                fprintf(stream, "%d", sqlite3_column_int(stmt, i));
                break;
            case SQLITE_FLOAT:
                fprintf(stream, "%f", sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT:
                fprintf(stream, "\"%s\"", sqlite3_column_text(stmt, i));
                break;
            case SQLITE_BLOB:
                fprintf(stream, "(BLOB)");
                break;
            case SQLITE_NULL:
                fprintf(stream, "NULL");
                break;
            default:
                fprintf(stream, "???");
                fprintf(stderr, "error: unknown sql data type with value `%d`\n", col_type);
                break;
            }
            fprintf(stream, " ");
        }
        fprintf(stream, "\n");
    }

    if (rc != SQLITE_DONE) {
        fprintf(stderr, "error: failed to execute statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
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
    fprintf(stream, "usage: %s -p <port> [-d <database_filepath>] [-h]\n", program);
}

int main(int argc, char **argv)
{
    const char *const program = shift(&argc, &argv);
    const char *port_as_cstr = NULL;
    const char *database_filepath = NULL;

    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "-p") == 0) {
            if (argc == 0) {
                fprintf(stderr, "error: missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            port_as_cstr = shift(&argc, &argv);
        } else if (strcmp(flag, "-d") == 0) {
            if (argc == 0) {
                fprintf(stderr, "error: missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            database_filepath = shift(&argc, &argv);
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

    if (database_filepath == NULL) {
        if (mkdir(DEFAULT_DATABASE_FILEPATH, 0777) == -1) {
            if (errno != EEXIST) {
                fprintf(stderr, "error: could not create directory `%s`: %s\n",
                        DEFAULT_DATABASE_FILEPATH, strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            printf("created database directory `%s`\n", DEFAULT_DATABASE_FILEPATH);
        }
        database_filepath = DEFAULT_DATABASE_FILEPATH"/default.db";
    }

    if (sqlite3_open(database_filepath, &server_db) != SQLITE_OK) {
        fprintf(stderr, "error: cannot open database `%s`: %s\n", database_filepath, sqlite3_errmsg(server_db));
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    if (!db_verify_tables(server_db, database_filepath)) {
        fprintf(stderr, "error: database verification failed\n");
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    // TEMP: print blacklisted ip addresses found in the database
    const char *sql_select_all_blacklisted_ips = "SELECT * FROM ip_blacklist;";
    if (!db_print_query_result(stdout, server_db, sql_select_all_blacklisted_ips)) {
        fprintf(stderr, "error: failed to execute query `%s`\n", sql_select_all_blacklisted_ips);
        sqlite3_close(server_db);
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

    sqlite3_close(server_db);

    pollfd_set_shutdown(&server_pfds);

    if (close(server_socket) == -1) {
        fprintf(stderr, "error while closing the socket: %s\n", strerror(errno));
    }

    return EXIT_SUCCESS;
}
