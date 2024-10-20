#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sqlite3.h>

#include <glm/gtc/type_ptr.hpp>

#include "pollfd_set.h"
#include "common/log.h"
#include "common/clock.h"
#include "common/asserts.h"
#include "common/defines.h"
#include "common/hexdump.h"
#include "common/packet.h"
#include "common/player_types.h"
#include "uthash/uthash.h"

#define SERVER_BACKLOG 10
#define INPUT_BUFFER_SIZE 1024
#define INPUT_OVERFLOW_BUFFER_SIZE 1024
#define DEFAULT_DATABASE_FILEPATH "db"

typedef struct {
    i32 socket;
    player_id id;
    UT_hash_handle hh;
} socket_player_id_pair_t;

LOCAL bool running;
LOCAL i32 server_socket;
LOCAL pollfd_set_t server_pfds;
LOCAL sqlite3 *server_db = NULL;
LOCAL player_t *players = NULL; // uthash
LOCAL socket_player_id_pair_t *socket_to_player_id_map = NULL; // uthash
LOCAL player_id player_next_id = 1000;

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

bool check_ip_allowed(const char *ip_addr)
{
    ASSERT_MSG(server_db != NULL, "sqlite3 handle not initialized");

    char sql[256] = {0};
    snprintf(sql, 256, "SELECT * FROM ip_blacklist WHERE ip_address = '%s';", ip_addr);

    sqlite3_stmt *stmt;
    i32 rc = sqlite3_prepare_v2(server_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to prepare statement: %s\n", sqlite3_errmsg(server_db));
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc != SQLITE_ROW;
}

bool player_authenticate(const char *username, const char *password)
{
    ASSERT_MSG(server_db != NULL, "sqlite3 handle not initialized");

    char sql[256] = {0};
    snprintf(sql, 256, "SELECT * FROM users WHERE username = '%s' AND password = '%s';", username, password);

    sqlite3_stmt *stmt;
    i32 rc = sqlite3_prepare_v2(server_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to prepare statement: %s\n", sqlite3_errmsg(server_db));
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

bool is_player_already_connected(const char *username, u8 length)
{
    // TODO: Optimize
    // Not ideal to iterate through all players and compare strings.
    for (player_t *player = players; player != NULL; player = (player_t *) player->hh.next) {
        if (strncmp(username, player->username, length) == 0) {
            return true;
        }
    }

    return false;
}

bool validate_incoming_client(i32 client_socket)
{
    u64 puzzle_value = clock_get_absolute_time_ns();

    i64 bytes_read, bytes_sent;

    bytes_sent = send(client_socket, (void *) &puzzle_value, sizeof(puzzle_value), 0);
    if (bytes_sent == -1) {
        LOG_ERROR("validation: send error: %s\n", strerror(errno));
        return false;
    } else if (bytes_sent != sizeof(puzzle_value)) {
        LOG_ERROR("validation: failed to send %lu bytes of validation data\n", sizeof(puzzle_value));
        return false;
    }

    // TODO: Come up with a better validation function
    u64 answer = puzzle_value ^ 0xDEADBEEFCAFEBABE;

    u64 result_buffer;
    bytes_read = recv(client_socket, (void *) &result_buffer, sizeof(result_buffer), 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            LOG_ERROR("validation: recv error: %s\n", strerror(errno));
        } else if (bytes_read == 0) {
            LOG_ERROR("validation: orderly shutdown\n");
        }
        return false;
    }

    if (bytes_read == sizeof(result_buffer)) {
        bool status_buffer = result_buffer == answer;
        bytes_sent = send(client_socket, (void *) &status_buffer, sizeof(status_buffer), 0);
        if (bytes_sent == -1) {
            LOG_ERROR("validation status: send error: %s\n", strerror(errno));
            return false;
        } else if (bytes_sent != sizeof(status_buffer)) {
            LOG_ERROR("validation status: failed to send %lu bytes of validation data\n", sizeof(puzzle_value));
            return false;
        }

        return status_buffer;
    }

    LOG_WARN("validation: received incorrect number of bytes\n");
    return false;
}

void handle_new_connection_request_event(void)
{
    struct sockaddr_storage client_addr;
    u32 client_addr_len = sizeof(client_addr);

    i32 client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        LOG_ERROR("accept error: %s\n", strerror(errno));
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
    UNUSED(port);
    LOG_INFO("new connection from %s:%hu\n", client_ip, port);

    if (!check_ip_allowed(client_ip)) {
        LOG_WARN("rejected connection from %s because the ip address is blacklisted\n", client_ip);
        close(client_socket);
        return;
    }

    LOG_INFO("ip address %s is allowed\n", client_ip);

    if (!validate_incoming_client(client_socket)) {
        LOG_WARN("%s:%hu failed validation\n", client_ip, port);
        close(client_socket);
        return;
    }

    LOG_INFO("%s:%hu passed validation\n", client_ip, port);

    pollfd_set_add(&server_pfds, client_socket);
}

LOCAL glm::vec3 get_random_color(void)
{
    PERSIST bool seeded = false;
    if (!seeded) {
        srand((u32) time(NULL));
        seeded = true;
    }

    f32 r = (f32) rand() / (f32) RAND_MAX;
    f32 g = (f32) rand() / (f32) RAND_MAX;
    f32 b = (f32) rand() / (f32) RAND_MAX;

    return glm::vec3(r, g, b);
}

LOCAL void handle_player_join_request(i32 client_socket, packet_player_join_req_t *packet)
{
    ASSERT(packet->username_length <= PLAYER_USERNAME_MAX_LEN && packet->password_length <= PLAYER_PASSWORD_MAX_LEN);

    char username[PLAYER_USERNAME_MAX_LEN + 1] = {};
    char password[PLAYER_PASSWORD_MAX_LEN + 1] = {};
    memcpy(username, packet->username, packet->username_length);
    memcpy(password, packet->password, packet->password_length);

    packet_player_join_res_t response = {};
    if (!player_authenticate(username, password)) {
        LOG_WARN("player authentication failed for `%s`\n", username);
        response.approved = false;
        if (!packet_send(client_socket, PACKET_TYPE_PLAYER_JOIN_RES, &response)) {
            LOG_ERROR("failed to send player join response packet\n");
        }
        return;
    }

    if (is_player_already_connected(username, packet->username_length)) {
        LOG_WARN("player `%s` already connected\n", username);
        response.approved = false;
        if (!packet_send(client_socket, PACKET_TYPE_PLAYER_JOIN_RES, &response)) {
            LOG_ERROR("failed to send player join response packet\n");
        }
        return;
    }

    LOG_INFO("player authentication succeeded for `%s`\n", username);

    glm::vec3 color = get_random_color();
    glm::vec3 position = glm::vec3(0.0f);

    player_t *new_player = (player_t *) malloc(sizeof(player_t));
    new_player->socket = client_socket;
    new_player->id = player_next_id++;
    memcpy(new_player->username, packet->username, packet->username_length);
    new_player->color = color;
    new_player->position = position;

    response.approved = true;
    response.id = new_player->id;
    memcpy(response.color, glm::value_ptr(new_player->color), 3 * sizeof(f32));
    memcpy(response.position, glm::value_ptr(new_player->position), 3 * sizeof(f32));

    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_JOIN_RES, &response)) {
        LOG_ERROR("failed to send player join response packet\n");
    }

    {
        // Send new player to all existing players
        packet_player_add_t player_add = {};
        player_add.id = new_player->id;
        player_add.username_length = packet->username_length;
        memcpy(player_add.username, new_player->username, player_add.username_length);
        memcpy(player_add.color, glm::value_ptr(new_player->color), 3 * sizeof(f32));
        memcpy(player_add.position, glm::value_ptr(new_player->position), 3 * sizeof(f32));

        player_t *player;
        for (player = players; player != NULL; player = (player_t *) player->hh.next) {
            if (!packet_send(player->socket, PACKET_TYPE_PLAYER_ADD, &player_add)) {
                LOG_ERROR("failed to send new player to existing player socket=%d id=%u\n", player->socket, player->id);
            }
        }
    }

    {
        // Send all existing players to the new player
        player_t *player;
        for (player = players; player != NULL; player = (player_t *) player->hh.next) {
            packet_player_add_t player_add = {};
            player_add.id = player->id;
            player_add.username_length = (u8) strlen(player->username);
            memcpy(player_add.username, player->username, player_add.username_length);
            memcpy(player_add.color, glm::value_ptr(player->color), 3 * sizeof(f32));
            memcpy(player_add.position, glm::value_ptr(player->position), 3 * sizeof(f32));

            if (!packet_send(client_socket, PACKET_TYPE_PLAYER_ADD, &player_add)) {
                LOG_ERROR("failed to send player add packet\n");
            }
        }
    }

    // Add at the end so that the iteration above does not include the new player.
    HASH_ADD_INT(players, id, new_player);

    // Add mapping of client socket to player id
    socket_player_id_pair_t *mapping = (socket_player_id_pair_t *) malloc(sizeof(socket_player_id_pair_t));
    mapping->socket = client_socket;
    mapping->id = new_player->id;
    HASH_ADD_INT(socket_to_player_id_map, socket, mapping);
    LOG_DEBUG("added mapping between socket=%d -> player_id=%u\n", mapping->socket, mapping->id);
}

LOCAL void process_network_packet(i32 client_socket, u32 type, void *data)
{
    switch (type) {
        case PACKET_TYPE_NONE: {
            LOG_WARN("ignoring received packet of type PACKET_TYPE_NONE\n");
        } break;
        case PACKET_TYPE_PING: {
            LOG_TRACE("received ping packet\n");
            packet_send(client_socket, PACKET_TYPE_PING, (packet_ping_t *) data);
        } break;
        case PACKET_TYPE_TXT_MSG: {
            packet_txt_msg_t packet;
            deserialize_packet_txt_msg(data, &packet);
            LOG_TRACE("received text message of length %lu: %s\n", packet.length, packet.message);
        } break;
        case PACKET_TYPE_PLAYER_JOIN_REQ: {
            packet_player_join_req_t *packet = (packet_player_join_req_t *) data;
            handle_player_join_request(client_socket, packet);
        } break;
        case PACKET_TYPE_PLAYER_REMOVE: {
            packet_player_remove_t *remove = (packet_player_remove_t *) data;

            player_t *player;
            for (player = players; player != NULL; player = (player_t *) player->hh.next) {
                // Do not send remove packet to the player who is disconnecting (client_socket)
                if (player->socket != client_socket) {
                    if (!packet_send(player->socket, PACKET_TYPE_PLAYER_REMOVE, remove)) {
                        LOG_ERROR("failed to send player remove packet\n");
                    }
                }
            }

            socket_player_id_pair_t *mapping = NULL;
            HASH_FIND_INT(socket_to_player_id_map, &client_socket, mapping);
            ASSERT(mapping != NULL);
            LOG_DEBUG("removed mapping between socket=%d -> player_id=%u\n", mapping->socket, mapping->id);
            HASH_DEL(socket_to_player_id_map, mapping);
            free(mapping);

            player_t *p;
            HASH_FIND_INT(players, &remove->id, p);
            if (p) {
                HASH_DEL(players, p);
                free(p);
                LOG_DEBUG("removed player with id=%u from server\n", remove->id);
            } else {
                LOG_ERROR("player with id=%u not found\n", remove->id);
            }

            pollfd_set_remove(&server_pfds, client_socket);
            close(client_socket);
        } break;
        case PACKET_TYPE_PLAYER_MOVE: {
            packet_player_move_t *packet = (packet_player_move_t *) data;

            // Update locally
            player_t *player;
            HASH_FIND_INT(players, &packet->id, player);
            if (player != NULL) {
                memcpy(glm::value_ptr(player->position), packet->position, 3 * sizeof(f32));
            } else {
                LOG_ERROR("player with id=%u not found\n", packet->id);
                break;
            }

            // Send updates to all existing players
            for (player = players; player != NULL; player = (player_t *) player->hh.next) {
                // Do not send move packet to the player who is sending the update
                if (player->socket != client_socket) {
                    if (!packet_send(player->socket, PACKET_TYPE_PLAYER_MOVE, packet)) {
                        LOG_ERROR("failed to send player move packet\n");
                    }
                }
            }
        } break;
        default: {
            LOG_ERROR("unknown packet type value `%u`\n", type);
        }
    }
}

bool receive_client_data(i32 client_socket, u8 *recv_buffer, u32 buffer_size, i64 *bytes_read)
{
    ASSERT(client_socket > 2 && recv_buffer && bytes_read);

    *bytes_read = recv(client_socket, recv_buffer, buffer_size, 0);
    if (*bytes_read <= 0) {
        if (*bytes_read == -1) {
            LOG_ERROR("recv error: %s\n", strerror(errno));
        } else if (*bytes_read == 0) {
            LOG_INFO("orderly shutdown of client with socket=%d\n", client_socket);

            // Update other players that the user disconnected if the mapping exists
            socket_player_id_pair_t *mapping;
            HASH_FIND_INT(socket_to_player_id_map, &client_socket, mapping);

            if (mapping != NULL) {
                LOG_DEBUG("removed mapping between socket=%d -> player_id=%u\n", mapping->socket, mapping->id);
                packet_player_remove_t remove = { .id = mapping->id };

                player_t *player;
                for (player = players; player != NULL; player = (player_t *) player->hh.next) {
                    // Do not send remove packet to the player who is disconnecting (client_socket)
                    if (player->socket != client_socket) {
                        if (!packet_send(player->socket, PACKET_TYPE_PLAYER_REMOVE, &remove)) {
                            LOG_ERROR("failed to send player remove packet\n");
                        }
                    }
                }

                HASH_FIND_INT(players, &mapping->id, player);
                if (player) {
                    HASH_DEL(players, player);
                    free(player);
                    LOG_DEBUG("removed player with id=%u from server\n", mapping->id);
                } else {
                    LOG_ERROR("player with id=%u not found\n", mapping->id);
                }

                HASH_DEL(socket_to_player_id_map, mapping);
                free(mapping);
            }

            pollfd_set_remove(&server_pfds, client_socket);
            close(client_socket);
        }

        return false;
    }

    return true;
}

void handle_client_event(i32 client_socket)
{
    u8 recv_buffer[INPUT_BUFFER_SIZE + INPUT_OVERFLOW_BUFFER_SIZE] = {0};
    i64 bytes_read;
    if (!receive_client_data(client_socket, recv_buffer, INPUT_BUFFER_SIZE, &bytes_read)) {
        return;
    }

#if 0
    LOG_DEBUG("read %lld bytes of data:\n", bytes_read);
    hexdump(stdout, recv_buffer, bytes_read, HEXDUMP_FLAG_CANONICAL);
#endif

    PERSIST const u32 header_size = PACKET_TYPE_SIZE[PACKET_TYPE_HEADER];

    ASSERT_MSG(bytes_read >= header_size, "unhandled: read fewer bytes than the header size");

    i64 remaining_bytes_to_parse = bytes_read;
    u8 *bufptr = recv_buffer;

    // Iterate over all the packets included in single TCP data reception.
    // Optionally, read more data to complete the payload.
    for (;;) {
        packet_header_t *header = (packet_header_t *) bufptr;

        if (header->type >= NUM_OF_PACKET_TYPES) {
            LOG_ERROR("received unknown header type `%u`\n", header->type);
            break;
        }

        // Verify whether all the payload data is present in the receive buffer.
        // If it’s not, read the remaining bytes from the socket that are needed to complete the payload.
        if (remaining_bytes_to_parse - header_size < header->payload_size) {
            i64 missing_bytes = header->payload_size - (remaining_bytes_to_parse - header_size);

            LOG_DEBUG("reading %lu more bytes to complete payload\n", missing_bytes);

            ASSERT_MSG(missing_bytes <= INPUT_OVERFLOW_BUFFER_SIZE, "not enough space in overflow buffer");

            // Read into INPUT_OVERFLOW_BUFFER of the recv_buffer and proceed to packet interpretation.
            i64 new_bytes_read = recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], missing_bytes, 0);
            UNUSED(new_bytes_read);
            ASSERT(new_bytes_read == missing_bytes);
        }

        process_network_packet(client_socket, header->type, bufptr + header_size);

        // Check if there are more bytes to parse.
        u64 parsed_packet_size = header_size + header->payload_size;
        bufptr = (bufptr + parsed_packet_size);
        remaining_bytes_to_parse -= parsed_packet_size;
        if (remaining_bytes_to_parse > 0) {
            LOG_DEBUG("remaining_bytes_to_parse = %lu\n", remaining_bytes_to_parse);
            if (remaining_bytes_to_parse >= header_size) {
                // Enough unparsed bytes to read the header.
                packet_header_t *next_header = (packet_header_t *) bufptr;
                if (next_header->type <= PACKET_TYPE_NONE || next_header->type >= NUM_OF_PACKET_TYPES) {
                    break;
                }
                LOG_DEBUG("next header is valid\n");
            } else {
                // Not enough remaining unparsed bytes to read a complete header.
                ASSERT_MSG(0, "unhandled: fewer bytes than header size");
            }
        } else {
            // No more bytes to parse.
            break;
        }
    }
}

LOCAL bool db_execute_sql(sqlite3 *db, const char *const sql)
{
    ASSERT(db != NULL);

    char *err = NULL;
    i32 rc = sqlite3_exec(db, sql, 0, 0, &err);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to execute sql query: %s\n", err);
        sqlite3_free(err);
        return false;
    }

    return true;
}

LOCAL bool db_table_exists(sqlite3 *db, const char *table_name)
{
    ASSERT(db != NULL);

    char sql[256] = {0};
    snprintf(sql, sizeof(sql), "SELECT name FROM sqlite_master WHERE type='table' AND name='%s';", table_name);

    sqlite3_stmt *stmt;
    i32 rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_ROW;
}

LOCAL bool db_verify_tables(sqlite3 *db)
{
    ASSERT(db != NULL);

    LOG_INFO("verifying database tables\n");

    if (!db_table_exists(db, "ip_blacklist")) {
        const char *sql_create_ip_blacklist_table =
            "CREATE TABLE ip_blacklist (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    ip_address TEXT NOT NULL UNIQUE,\n"
            "    description TEXT,\n"
            "    created_at DATETIME DEFAULT (DATETIME('now', 'localtime'))\n"
            ");";

        LOG_INFO("creating 'ip_blacklist' table...");
        if (!db_execute_sql(db, sql_create_ip_blacklist_table)) {
            printf(ERROR_COLOR " ERROR" RESET_COLOR "\n");
            return false;
        } else {
            printf(INFO_COLOR " OK" RESET_COLOR "\n");
        }
    }

    if (!db_table_exists(db, "users")) {
        const char *sql_create_users_table =
            "CREATE TABLE users (\n"
            "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
            "    username TEXT NOT NULL UNIQUE,\n"
            "    password TEXT NOT NULL,\n"
            "    created_at DATETIME DEFAULT (DATETIME('now', 'localtime'))\n"
            ");";

        LOG_INFO("creating 'users' table...");
        if (!db_execute_sql(db, sql_create_users_table)) {
            printf(ERROR_COLOR " ERROR" RESET_COLOR "\n");
            return false;
        } else {
            printf(INFO_COLOR " OK" RESET_COLOR "\n");
        }
    }

    LOG_INFO("database verification completed successfully\n");
    return true;
}

LOCAL bool db_print_query_result(FILE *stream, sqlite3 *db, const char *const sql)
{
    ASSERT(db != NULL);

    sqlite3_stmt *stmt;
    i32 rc;

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR("failed to prepare stetement: %s\n", sqlite3_errmsg(db));
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
                LOG_ERROR("unknown sql data type with value `%d`\n", col_type);
                break;
            }
            fprintf(stream, " ");
        }
        fprintf(stream, "\n");
    }

    if (rc != SQLITE_DONE) {
        LOG_ERROR("failed to execute statement: %s\n", sqlite3_errmsg(db));
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

LOCAL char *shift(int *argc, char ***argv)
{
    ASSERT(*argc > 0);
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
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            port_as_cstr = shift(&argc, &argv);
        } else if (strcmp(flag, "-d") == 0) {
            if (argc == 0) {
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            database_filepath = shift(&argc, &argv);
        } else if (strcmp(flag, "-h") == 0) {
            usage(stdout, program);
            exit(EXIT_SUCCESS);
        } else {
            LOG_FATAL("unknown flag `%s`\n", flag);
            usage(stderr, program);
            exit(EXIT_FAILURE);
        }
    }

    if (port_as_cstr == NULL) {
        LOG_FATAL("port argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    }

    if (database_filepath == NULL) {
        if (mkdir(DEFAULT_DATABASE_FILEPATH, 0777) == -1) {
            if (errno != EEXIST) {
                LOG_FATAL("could not create directory `%s`: %s\n",
                          DEFAULT_DATABASE_FILEPATH, strerror(errno));
                exit(EXIT_FAILURE);
            }
        } else {
            LOG_INFO("created database directory `%s`\n", DEFAULT_DATABASE_FILEPATH);
        }
        database_filepath = DEFAULT_DATABASE_FILEPATH"/default.db";
    }

    if (sqlite3_open(database_filepath, &server_db) != SQLITE_OK) {
        LOG_FATAL("cannot open database `%s`: %s\n", database_filepath, sqlite3_errmsg(server_db));
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    if (!db_verify_tables(server_db)) {
        LOG_FATAL("database verification failed\n");
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    // TEMP: print blacklisted ip addresses found in the database
    const char *sql_select_all_blacklisted_ips = "SELECT * FROM ip_blacklist;";
    if (!db_print_query_result(stdout, server_db, sql_select_all_blacklisted_ips)) {
        LOG_FATAL("failed to execute query `%s`\n", sql_select_all_blacklisted_ips);
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    // TEMP: print users found in the database
    const char *sql_select_users = "SELECT * FROM users;";
    if (!db_print_query_result(stdout, server_db, sql_select_users)) {
        LOG_FATAL("failed to execute query `%s`\n", sql_select_users);
        sqlite3_close(server_db);
        exit(EXIT_FAILURE);
    }

    char hostname[256] = {0};
    gethostname(hostname, 256);
    LOG_INFO("starting the game server on host `%s`\n", hostname);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_PASSIVE;

    i32 status = getaddrinfo(NULL, port_as_cstr, &hints, &result);
    if (status != 0) {
        LOG_FATAL("getaddrinfo error: %s\n", gai_strerror(status));
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
        UNUSED(ip_version); UNUSED(socktype_str);
        LOG_INFO("successfully created an %s %s socket(fd=%d)\n", ip_version, socktype_str, server_socket);

        PERSIST i32 yes = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(i32)) == -1) {
            LOG_ERROR("setsockopt reuse address error: %s\n", strerror(errno));
        }

        if (bind(server_socket, rp->ai_addr, rp->ai_addrlen) == 0) {
            LOG_INFO("successfully bound socket(fd=%d)\n", server_socket);
            break;
        } else {
            LOG_ERROR("failed to bind socket with fd=%d\n", server_socket);
            close(server_socket);
        }
    }

    if (rp == NULL) {
        LOG_FATAL("could not bind to port %s\n", port_as_cstr);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SERVER_BACKLOG) == -1) {
        LOG_FATAL("failed to start listening: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        char ip_buffer[INET6_ADDRSTRLEN] = {0};
        inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), ip_buffer, INET6_ADDRSTRLEN);
        LOG_INFO("socket listening on %s port %s\n", ip_buffer, port_as_cstr);
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
                LOG_INFO("interrupted 'poll' system call\n");
                break;
            }
            LOG_FATAL("poll error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (u32 i = 0; i < server_pfds.count; i++) {
            if (server_pfds.fds[i].revents & POLLIN) {
                if (server_pfds.fds[i].fd == server_socket) {
                    handle_new_connection_request_event();
                } else {
                    handle_client_event(server_pfds.fds[i].fd);
                }
            }
        }
    }

    LOG_INFO("server shutting down\n");

    sqlite3_close(server_db);

    pollfd_set_shutdown(&server_pfds);

    {
        // uthash cleanup
        player_t *p, *tmp1;
        HASH_ITER(hh, players, p, tmp1) {
            HASH_DEL(players, p);
            free(p);
        }

        socket_player_id_pair_t *s, *tmp2;
        HASH_ITER(hh, socket_to_player_id_map, s, tmp2) {
            HASH_DEL(socket_to_player_id_map, s);
            free(s);
        }
    }

    if (close(server_socket) == -1) {
        LOG_ERROR("error while closing the socket: %s\n", strerror(errno));
    }

    return EXIT_SUCCESS;
}
