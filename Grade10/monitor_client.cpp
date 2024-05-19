#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 1024

int client_socket;

void signal_handler(int signal) {
    printf("Caught signal %d, terminating monitor client...\n", signal);
    close(client_socket);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <SERVER_IP> <PORT>\n", argv[0]);
        return -1;
    }

    const char *SERVER_IP = argv[1];
    int PORT = atoi(argv[2]);

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_address.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        return -1;
    }

    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        return -1;
    }

    printf("Connected to server.\n");


    const char *handshake_message = "MONITOR";
    if (send(client_socket, handshake_message, strlen(handshake_message), 0) == -1) {
        perror("Handshake message send failed");
        close(client_socket);
        return -1;
    }

    signal(SIGINT, signal_handler);

    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("SERVER LOG: %s\n", buffer);
    }

    if (bytes_received == -1) {
        perror("Receive failed");
    }

    close(client_socket);
    printf("Connection closed.\n");

    return 0;
}
