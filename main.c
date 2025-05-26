#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include "semaphores.h"
#include "server_socket.h"

#define LISTEN_BACKLOG 128
#define MAX_CONNECTIONS 5
#define PORT 2000
#define IP ""

void* processSocketRequests(int acceptedSocketFD, int connectionSemaphore);

int main() {
    printf("[+] starting with IP: '%s' and PORT: %d\n", IP, PORT);

    const int serverSocketFD = createIPv4Socket();
    if (serverSocketFD == -1) {
        perror("[!] error while opening socket\n");
        return -1;
    }

    struct sockaddr_in* serverAddress = createIPv4Address(IP, PORT);
    if ( serverAddress == NULL) {
        perror("[!] error while creating IPv4 Address\n");
        free(serverAddress);

        return -1;
    };

    int bindResult = bind(serverSocketFD, (struct sockaddr*)serverAddress, sizeof(*serverAddress)); // Attach socket (process) to port
    if (bindResult == -1) {
        perror("[!] error while binding socket\n");
        free(serverAddress);

        return 0;
    }

    free(serverAddress);

    printf("[+] socket was bound successfully\n");

    int listenResult = listen(serverSocketFD, LISTEN_BACKLOG);
    if (listenResult == -1) {
        perror("[!] error while listening incoming connections\n");

        return 0;
    }

    int semaphoreConnectionsID = createSemaphore(MAX_CONNECTIONS);

    startAcceptingIncomingConnections(serverSocketFD, semaphoreConnectionsID, processSocketRequests);

    printf("[!] shutting down....");
    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void* processSocketRequests(int acceptedSocketFD, int connectionSemaphore) {
    char buffer[1024];

    while (true) {
        ssize_t amountReceived = recv(acceptedSocketFD, buffer, sizeof(buffer), 0);
        if (amountReceived < 0) {
            char errorMessage[1024];

            sprintf(errorMessage, "[-] fd: %d - error while receiving data\n", acceptedSocketFD);
            perror(errorMessage);

            break;
        }

        if (amountReceived == 0) {
            printf("[-] fd: %d - socket closed\n", acceptedSocketFD);
            break;
        }

        buffer[amountReceived] = '\0';
        printf("[+] fd: %d - received message: %s\n", acceptedSocketFD, buffer);

        char genericResponse[] = "ok";
        send(acceptedSocketFD, genericResponse, sizeof (genericResponse), 0);
    }

    close(acceptedSocketFD);
    VSemaphore(connectionSemaphore);

    return NULL;
}