#include "server_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>


#include "../semaphores/semaphores.h"

// Colors
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KBLU  "\x1B[34m"
#define KCYN  "\x1B[36m"

#define MAX_CONNECTIONS_SUPPORT 100

typedef struct {
    pthread_t threads[MAX_CONNECTIONS_SUPPORT];
    int clientSocketFD[MAX_CONNECTIONS_SUPPORT];
    pthread_mutex_t* mutex;
} ActiveThreads;

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
    if (GetValSemaphore(params->availableConnectionsSemaphore) > MAX_CONNECTIONS_SUPPORT) {
        perror("max available connections above max connections support");
        return NULL;
    }

    ActiveThreads activeThreads;
    activeThreads.mutex = newMutex();
    for (int i = 0; i < MAX_CONNECTIONS_SUPPORT; i++) {
        activeThreads.threads[i] = NULL;
        activeThreads.clientSocketFD[i] = 0;
    }

    while (true) {
        PSemaphore(params->availableConnectionsSemaphore);
        // Critic Region
        AcceptedSocket* acceptedSocket = acceptIncomingConnection(params->serverSocketFD);
        if (!acceptedSocket->acceptedSuccessfully) {
            free(acceptedSocket);
            printf("%s[!] accept connections interrupted, socket closed \n",KRED);

            for (int i = 0; i < MAX_CONNECTIONS_SUPPORT; i++) {
                if (activeThreads.threads[i] != NULL) {
                    pthread_mutex_lock(activeThreads.mutex);

                    shutdown(activeThreads.clientSocketFD[i], SHUT_RDWR);
                    close(activeThreads.clientSocketFD[i]);
                    printf("%s[*] Closed client connection socket %d.\n",KCYN, activeThreads.clientSocketFD[i]);

                    int clientSocketFD = activeThreads.clientSocketFD[i];
                    pthread_t thread = activeThreads.threads[i];
                    pthread_mutex_unlock(activeThreads.mutex);

                    printf("%s[*]Waiting thread to finish (fd %d)...\n",KCYN, clientSocketFD);
                    pthread_join(thread, NULL);
                    printf("%s[*]thread finished (fd %d)...\n",KCYN, clientSocketFD);
                }
            }

            printf("%s[*] every socket active thread finished \n",KGRN);


            break;
        }

        pthread_mutex_lock(params->establishedConnections->mutex);
        pthread_mutex_lock(activeThreads.mutex);

        params->establishedConnections->count += 1;
        params->establishedConnections->firstClientConnected = true;

        pthread_t id;
        void* argumentsToProcessSocket[5] = {acceptedSocket, &(params->availableConnectionsSemaphore), params->establishedConnections, params->callback, &activeThreads};
        pthread_create(&id, NULL, processCallback, argumentsToProcessSocket);

        for (int i = 0; i < MAX_CONNECTIONS_SUPPORT; i++) {
            if (activeThreads.threads[i] == NULL) {
                activeThreads.threads[i] = id;
                activeThreads.clientSocketFD[i] = acceptedSocket->acceptedSocketFD;
                break;
            }
        }

        char connectionIP[16];
        inet_ntop(AF_INET, &acceptedSocket->address.sin_addr, connectionIP, 16);
        printf("%s[+] new connection accepted with fd: %d and address: %s. Sent to thread: %p\n", KGRN, acceptedSocket->acceptedSocketFD, connectionIP, id);

        pthread_mutex_unlock(activeThreads.mutex);
        pthread_mutex_unlock(params->establishedConnections->mutex);
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

    void* activeThreadsPointer = ((void**)args)[4];
    ActiveThreads* activeThreads = (ActiveThreads*)activeThreadsPointer;

    callback(acceptedSocketFD, availableConnectionsSemaphore);

    close(acceptedSocketFD);

    pthread_mutex_lock(establishedConnections->mutex);
    pthread_mutex_lock(activeThreads->mutex);

    // Liberar ID de hilos activos
    for (int i = 0; i < MAX_CONNECTIONS_SUPPORT; i++) {
        if (pthread_equal(activeThreads->threads[i], pthread_self())) {
            printf("%s[*]Liberando hilo activo %p\n", KBLU, pthread_self());
            activeThreads->threads[i] = NULL;
            activeThreads->clientSocketFD[i] = 0;
        }
    }

    // Liberar conexiones establecidas
    establishedConnections->count -= 1;
    if (establishedConnections->count == 0) { // Avisar que ya no hay conexiones, apagar server
        pthread_mutex_unlock(establishedConnections->mutex);
        pthread_cond_signal(establishedConnections->noConnectionsCond);

        pthread_exit(NULL);
    }


    VSemaphore(availableConnectionsSemaphore);
    pthread_mutex_unlock(establishedConnections->mutex);
    pthread_mutex_unlock(activeThreads->mutex);

    pthread_exit(NULL);
}
