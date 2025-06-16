#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_CLIENTS 4
FILE *CLIENTS[MAX_CLIENTS] = {0};
// ^ Each connected client is represented as a FILE*
// If that client is not connected, its FILE* will be NULL

/** Sends the message `buf` to all connected clients except
 * the one at `sender_index`.
 * - Use `fprintf()` to send the message to a client
 * - Use `fflush()` after printing to the client to force sending out the message.
 * - If fprintf or fflush fail, close the client and reset that slot's FILE*
 *   to NULL.
 * */
void redistribute_message(int sender_index, char *buf);

/** Tries to read a message from the specified client.
 * Since the client has been made nonblocking, fgets will fail (by returning NULL)
 * if there is no data to read.
 * To distinguish this failure from a real one, check the value of `errno`.
 * The values EAGAIN or EWOULDBLOCK mean that there was simply no data to read.
 * If any other kind of error occurred, close the client and reset its FILE* to NULL.
 * Return a boolean indicating whether a message was actually read from the client.
 */
int poll_message(char *buf, size_t len, int client_index);

/** Tries to accept a new client connection.
 * Since the server socket is nonblocking, `accept` will signal failure by returning -1 in case
 * there are no pending connections.
 * Again, distinguish this case by checking the value of `errno` for the values EAGAIN and
 * EWOULDBLOCK.
 * - If a new connection is accepted, set it nonblocking by using `fcntl()`.
 * - Use `fdopen()` to convert the client file descriptor to a FILE*
 * - Try to find a spot in the CLIENTS array for the new connection.
 *      - If there is none, send a brief message to the new client saying so before closing the
 *        connection.
 * - If `accept` fails for a different reason, call `exit` to stop the server.
 */
void try_add_client(int server_fd);

int main_loop(int server_fd)
{
    char buf[1024];

    while (1) {
        // check each client to see if there are messages to redistribute
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (NULL == CLIENTS[i]) continue;
            if (!poll_message(buf, sizeof(buf), i)) continue;
            printf("Received message from client %i: %s", i, buf);
            redistribute_message(i, buf);
        }

        // see if there's a new client to add
        try_add_client(server_fd);

        usleep(100 * 1000); // wait 100ms before checking again
    }
}

int main(int argc, char* argv[])
{
    struct sockaddr_in address;
    int server_fd;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("socket");
        perror("socket");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 aka localhost
    // ^ this is a kind of security: the server will only listen
    // for incoming connections that come from 127.0.0.1 i.e. the same
    // computer that the server process is running on.
    address.sin_port = htons(7878);

    if (-1 == bind(server_fd, (struct sockaddr *)&address, sizeof(address))) {
        printf("bind");
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (-1 == listen(server_fd, 5)) {
        printf("listen");
        perror("listen");
        close(server_fd);
        return 1;
    }

    // set the server file descriptor to nonblocking mode
    // so that `accept` returns immediately if there are no pending
    // incoming connections
    if (-1 == fcntl(server_fd, F_SETFL, O_NONBLOCK)) {
        perror("fcntl server_fd NONBLOCK");
        close(server_fd);
        return 1;
    }

    int status = main_loop(server_fd);
    close(server_fd);
    return status;
}

/** Sends the message `buf` to all connected clients except
 * the one at `sender_index`.
 * - Use `fprintf()` to send the message to a client
 * - Use `fflush()` after printing to the client to force sending out the message.
 * - If fprintf or fflush fail, close the client and reset that slot's FILE*
 *   to NULL.
 * */
void redistribute_message(int sender_index, char *buf){
    for (int i =0; i<MAX_CLIENTS; i++){
        if (i == sender_index) continue;
        if (NULL == CLIENTS[i]) continue;
        if (-1 == fprintf(CLIENTS[i], "%s", buf)){
            fclose(CLIENTS[i]);
            CLIENTS[i] = NULL;
        }
        if (-1 == fflush(CLIENTS[i])){
            fclose(CLIENTS[i]);
            CLIENTS[i] = NULL;
        }
    }
}

/** Tries to read a message from the specified client.
 * Since the client has been made nonblocking, fgets will fail (by returning NULL)
 * if there is no data to read.
 * To distinguish this failure from a real one, check the value of `errno`.
 * The values EAGAIN or EWOULDBLOCK mean that there was simply no data to read.
 * If any other kind of error occurred, close the client and reset its FILE* to NULL.
 * Return a boolean indicating whether a message was actually read from the client.
 */
int poll_message(char *buf, size_t len, int client_index) {
    if (NULL == fgets(buf, len, CLIENTS[client_index])){
        if (errno != EAGAIN && errno != EWOULDBLOCK){
            fclose(CLIENTS[client_index]);
            CLIENTS[client_index] = NULL;
        }
        return 0;
    }
    return 1;
}

/** Tries to accept a new client connection.
 * Since the server socket is nonblocking, `accept` will signal failure by returning -1 in case
 * there are no pending connections.
 * Again, distinguish this case by checking the value of `errno` for the values EAGAIN and
 * EWOULDBLOCK.
 * - If a new connection is accepted, set it nonblocking by using `fcntl()`.
 * - Use `fdopen()` to convert the client file descriptor to a FILE*
 * - Try to find a spot in the CLIENTS array for the new connection.
 *      - If there is none, send a brief message to the new client saying so before closing the
 *        connection.
 * - If `accept` fails for a different reason, call `exit` to stop the server.
 */
void try_add_client(int server_fd){
    int client_fd;
    if (-1 == (client_fd = accept(server_fd, NULL, NULL))){
        if (errno != EAGAIN && errno != EWOULDBLOCK){
            exit(1);
        }
    }
    else {
        fcntl(client_fd, F_SETFL, O_NONBLOCK);
        FILE *client = fdopen(client_fd, "r+");
        int success  = 0;
        for (int i =0; i< MAX_CLIENTS; i++){
            if (CLIENTS[i] == 0){
                CLIENTS[i] = client;
                success=1;
                break;
            }
        }
        if (0 == success){
            fprintf(client, "connection full\n");
            fclose(client);
        }
    }
}
