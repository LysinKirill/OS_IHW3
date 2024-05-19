#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define DATABASE_SIZE 40
#define MAX_MONITOR_CLIENTS 5

int database[DATABASE_SIZE];
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writer_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_fd;
int monitor_clients[MAX_MONITOR_CLIENTS] = {-1};
pthread_mutex_t monitor_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_to_monitors(const char *message) {
    pthread_mutex_lock(&monitor_mutex);
    for (int i = 0; i < MAX_MONITOR_CLIENTS; ++i) {
        if (monitor_clients[i] != -1) {
            send(monitor_clients[i], message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&monitor_mutex);
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[1024] = {0};
    while (1) {
        int msg_len;
        int len_read = read(client_socket, &msg_len, sizeof(msg_len));
        if (len_read <= 0) {
            break;
        }

        if (msg_len > 0 && msg_len < sizeof(buffer)) {
            int total_read = 0;
            while (total_read < msg_len) {
                int valread = read(client_socket, buffer + total_read, msg_len - total_read);
                if (valread <= 0) {
                    break;
                }
                total_read += valread;
            }
            buffer[msg_len] = '\0';

            char *request = strdup(buffer);
            if (strncmp(request, "READ", 4) == 0) {
                int index = atoi(request + 5);
                int value;

                pthread_mutex_lock(&db_mutex);
                value = database[index];
                pthread_mutex_unlock(&db_mutex);

                char response[1024];
                snprintf(response, sizeof(response), "VALUE %d", value);
                send(client_socket, response, strlen(response), 0);

                snprintf(response, sizeof(response), "READ VALUE %d FROM CELL WITH INDEX %d", value, index);
                send_to_monitors(response);
            } else if (strncmp(request, "WRITE", 5) == 0) {
                int index, new_value;
                sscanf(request + 6, "%d %d", &index, &new_value);

                pthread_mutex_lock(&writer_mutex);
                pthread_mutex_lock(&db_mutex);
                database[index] = new_value;
                pthread_mutex_unlock(&db_mutex);
                pthread_mutex_unlock(&writer_mutex);

                char *response = "UPDATED";
                send(client_socket, response, strlen(response), 0);
                char response_log[1024];
                snprintf(response_log, sizeof(response_log), "WRITE VALUE %d TO INDEX %d", new_value, index);
                send_to_monitors(response_log);
            }
            free(request);
        } else {
            fprintf(stderr, "Invalid message length\n");
            break;
        }
    }
    close(client_socket);
    printf("Client disconnected.\n");
    char notification[1024];
    snprintf(notification, sizeof(notification), "CLIENT DISCONNECTED");
    send_to_monitors(notification);
    
    return NULL;
}

void signal_handler(int signal) {
    printf("Caught signal %d, terminating server...\n", signal);
    close(server_fd);
    exit(0);
}

int main(int argc, char const *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        return -1;
    }

    const char *SERVER_IP = argv[1];
    int PORT = atoi(argv[2]);

    for (int i = 0; i < DATABASE_SIZE; ++i) {
        database[i] = i + 1;
    }

    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(SERVER_IP);
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, signal_handler);

    printf("Server listening on %s:%d\n", SERVER_IP, PORT);

    while (1) {
        int *client_socket_ptr = (int *)malloc(sizeof(int));
        if (!client_socket_ptr) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        if ((*client_socket_ptr = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            free(client_socket_ptr);
            exit(EXIT_FAILURE);
        }
        printf("Client connected.\n");
        char notification[1024];
        snprintf(notification, sizeof(notification), "CLIENT CONNECTED");
        send_to_monitors(notification);


        char handshake_message[50];
        int bytes_received = recv(*client_socket_ptr, handshake_message, sizeof(handshake_message), 0);
        if (bytes_received > 0) {
            handshake_message[bytes_received] = '\0';
            if (strcmp(handshake_message, "MONITOR") == 0) {
                pthread_mutex_lock(&monitor_mutex);
                for (int i = 0; i < MAX_MONITOR_CLIENTS; ++i) {
                    if (monitor_clients[i] == -1) {
                        monitor_clients[i] = *client_socket_ptr;
                        break;
                    }
                }
                pthread_mutex_unlock(&monitor_mutex);
            } else {
                pthread_t client_thread;
                if (pthread_create(&client_thread, NULL, handle_client, client_socket_ptr) != 0) {
                    perror("pthread_create failed");
                    free(client_socket_ptr);
                    exit(EXIT_FAILURE);
                }
                pthread_detach(client_thread);
            }
        } else {
            perror("Error receiving handshake message");
            close(*client_socket_ptr);
            free(client_socket_ptr);
        }
    }

    close(server_fd);
    return 0;
}