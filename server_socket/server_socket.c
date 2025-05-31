#include "server_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>


#include "../semaphores/semaphores.h"

#define KGRN  "\x1B[32m"

void* processCallback(void* args);

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

void* startAcceptingIncomingConnections(void* param) {
    StartAcceptingConnectionsParams* params = (StartAcceptingConnectionsParams*) param;

    while (true) {
        PSemaphore(params->availableConnectionsSemaphore);
        // Critic Region
        AcceptedSocket* acceptedSocket = acceptIncomingConnection(params->serverSocketFD);
        if (!acceptedSocket->acceptedSuccessfully) {
            free(acceptedSocket);
            perror("[!] error while accepting incoming connection \n");

            break;
        }

        pthread_mutex_lock(params->establishedConnections->mutex);
        params->establishedConnections->count += 1;
        params->establishedConnections->firstClientConnected = true;
        pthread_mutex_unlock(params->establishedConnections->mutex);


        void* argumentsToProcessSocket[4] = {acceptedSocket, &(params->availableConnectionsSemaphore), params->establishedConnections, params->callback};
        pthread_t id;
        pthread_create(&id, NULL, processCallback, argumentsToProcessSocket);

        char connectionIP[16];
        inet_ntop(AF_INET, &acceptedSocket->address.sin_addr, connectionIP, 16);
        printf("%s[+] new connection accepted with fd: %d and address: %s. Sent to thread: %lu\n", KGRN, acceptedSocket->acceptedSocketFD, connectionIP, (unsigned long) id);
    }

    return NULL;
}

void* processCallback(void* args) {
    void* acceptedSocketPointer = ((void**)args)[0];
    int acceptedSocketFD = ((AcceptedSocket*)acceptedSocketPointer)->acceptedSocketFD;
    free(acceptedSocketPointer);

    void* availableConnectionsSemaphorePointer = ((void**)args)[1];
    int availableConnectionsSemaphore = *(int*)availableConnectionsSemaphorePointer;

    void* establishedConnectionsPointer = ((void**)args)[2];
    EstablishedConnections* establishedConnections = (EstablishedConnections*)establishedConnectionsPointer;

    void* callbackPointer = ((void**)args)[3];
    void*(*callback)(int, int) = callbackPointer;

    callback(acceptedSocketFD, availableConnectionsSemaphore);

    close(acceptedSocketFD);

    pthread_mutex_lock(establishedConnections->mutex);
    establishedConnections->count -= 1;
    if (establishedConnections->count == 0) { // Avisar que ya no hay conexiones, apagar server
        pthread_mutex_unlock(establishedConnections->mutex);
        pthread_cond_signal(establishedConnections->noConnectionsCond);

        pthread_exit(NULL);
    }
    pthread_mutex_unlock(establishedConnections->mutex);

    VSemaphore(availableConnectionsSemaphore);
    pthread_exit(NULL);
}
