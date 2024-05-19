#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define DATABASE_SIZE 40

typedef struct {
    int id;
    const char* SERVER_IP;
    int PORT;
} ReaderData;

int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1, c;
    for (int i = 2; i <= n; ++i) {
        c = a + b;
        a = b;
        b = c;
    }
    return b;
}

void signal_handler(int signal) {
    printf("Caught signal %d, terminating reader clients...\n", signal);
    exit(0);
}

void *reader_task(void *arg) {
    ReaderData *reader_data = (ReaderData *)arg;
    int id = reader_data->id;
    const char* SERVER_IP = reader_data->SERVER_IP;
    int PORT = reader_data->PORT;

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "Socket creation error\n");
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address / Address not supported\n");
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "Connection failed\n");
        return NULL;
    }


    const char *handshake_message = "READER";
    if (send(sock, handshake_message, strlen(handshake_message), 0) == -1) {
        perror("Handshake message send failed");
        close(sock);
        return NULL;
    }

    while (1) {
        int sleep_time = 1000 + rand() % 5000;
        usleep(sleep_time * 1000);
        int index = rand() % DATABASE_SIZE;
        char request[1024];
        sprintf(request, "READ %d", index);
        printf("Reader %d requesting: %s\n", id, request);

        int msg_len = strlen(request);
        if (send(sock, &msg_len, sizeof(msg_len), 0) != sizeof(msg_len)) {
            fprintf(stderr, "Reader %d failed to send message length\n", id);
            break;
        }

        if (send(sock, request, msg_len, 0) != msg_len) {
            fprintf(stderr, "Reader %d failed to send message\n", id);
            break;
        }

        memset(buffer, 0, sizeof(buffer)); 
        int bytes_received = read(sock, buffer, sizeof(buffer) - 1);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            if (strstr(buffer, "VALUE") == buffer) {
                int value = atoi(buffer + 6);
                int fib_value = fibonacci(value); 
                printf("Reader %d: Index %d, Value %d, Fibonacci %d\n", id, index, value, fib_value);
            } else {
                printf("Reader %d received unexpected response: %s\n", id, buffer);
            }
        } else {
            fprintf(stderr, "Reader %d read error\n", id);
            break;
        }
    }

    close(sock);
    printf("Reader %d finished.\n", id);
    return NULL;
}

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <SERVER_IP> <PORT> <NUM_READERS>\n", argv[0]);
        return -1;
    }

    const char* SERVER_IP = argv[1];
    int PORT = atoi(argv[2]);
    int NUM_READERS = atoi(argv[3]);

    srand(time(NULL));

    signal(SIGINT, signal_handler);

    pthread_t readers[NUM_READERS];
    ReaderData reader_data[NUM_READERS];
    for (int i = 0; i < NUM_READERS; ++i) {
        reader_data[i].id = i + 1;
        reader_data[i].SERVER_IP = SERVER_IP;
        reader_data[i].PORT = PORT;
        if (pthread_create(&readers[i], NULL, reader_task, &reader_data[i]) != 0) {
            fprintf(stderr, "Error creating reader thread\n");
            return -1;
        }
    }

    for (int i = 0; i < NUM_READERS; ++i) {
        pthread_join(readers[i], NULL);
    }

    return 0;
}
