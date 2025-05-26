#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

typedef struct {
    int acceptedSocketFD;
    struct sockaddr_in address;
    bool acceptedSuccessfully;
    int error;
} AcceptedSocket;

int createIPv4Socket();
struct sockaddr_in* createIPv4Address(char* ip, int port);
AcceptedSocket * acceptIncomingConnection(int serverSocketFD);
void startAcceptingIncomingConnections(int serverSocketFD);
void acceptConnectionsAndProcessRequests(int serverSocketFD);
void* processSocketRequests(void* acceptedSocket);

#define LISTEN_BACKLOG 1
#define PORT 2000
#define IP ""

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

    startAcceptingIncomingConnections(serverSocketFD);

    printf("[!] shutting down....");
    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

int createIPv4Socket() {
    /*
     * AF_INET = IPv4
     * SOCK_STREAM = TCP Protocol
     * 0 = Choose protocol based on two previous params
     */
    return socket(AF_INET, SOCK_STREAM, 0);
}

struct sockaddr_in* createIPv4Address(char *ip, int port) {
    struct sockaddr_in* address = malloc(sizeof(struct sockaddr_in));
    address->sin_family = AF_INET;
    address->sin_port = htons(port); // convert port number to big endian

    if (strlen(ip) == 0) {
        address->sin_addr.s_addr = INADDR_ANY; // listen to any incoming address
    } else {
        int ptonResult = inet_pton(AF_INET, ip, &address->sin_addr.s_addr); // presentation to network (parse ip string to int32 ip)
        if (ptonResult != 1) {
            perror("error presenting to network\n");
            free(address);

            return NULL;
        }
    }

    return address;
}

AcceptedSocket* acceptIncomingConnection(int serverSocketFD) {
    struct sockaddr_in clientAddress;
    int clientAddressLength = sizeof(clientAddress);
    int clientSocketFD = accept(serverSocketFD, (struct sockaddr*)&clientAddress, (socklen_t*)&clientAddressLength);

    AcceptedSocket * acceptedSocket = malloc(sizeof(AcceptedSocket));
    acceptedSocket->acceptedSocketFD = clientSocketFD;
    acceptedSocket->address = clientAddress;
    acceptedSocket->acceptedSuccessfully = clientSocketFD > 0;
    if (clientSocketFD <= 0)
        acceptedSocket->error = clientSocketFD;

    return acceptedSocket;
}

void startAcceptingIncomingConnections(int serverSocketFD) {
    while (true) {
        AcceptedSocket* acceptedSocket = acceptIncomingConnection(serverSocketFD);
        if (!acceptedSocket->acceptedSuccessfully) {
            free(acceptedSocket);
            perror("[!] error while accepting incoming connection \n");

            break;
        }

        pthread_t id;
        pthread_create(&id, NULL, processSocketRequests, acceptedSocket);

        char connectionIP[16];
        inet_ntop(AF_INET, &acceptedSocket->address.sin_addr, connectionIP, 16);
        printf("[+] new connection accepted with fd: %d and address: %s. Sent to thread: %lu\n", acceptedSocket->acceptedSocketFD, connectionIP, (unsigned long) id);
    }
}

void* processSocketRequests(void* acceptedSocket) {
    char buffer[1024];
    int acceptedSocketFD = ((AcceptedSocket*)acceptedSocket)->acceptedSocketFD;
    free(acceptedSocket);

    while (true) {
        ssize_t amountReceived = recv(acceptedSocketFD, buffer, sizeof(buffer), 0);
        if (amountReceived < 0) {
            perrror("[-] fd: %d - error while receiving data\n", acceptedSocketFD);
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

    return NULL;
}