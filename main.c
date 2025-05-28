#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "semaphores/semaphores.h"
#include "server_socket/server_socket.h"
#include "regex.h"
#include "buffers/buffers.h"
#include "operations/operations.h"

#define LISTEN_BACKLOG 128
#define MAX_CONNECTIONS 5
#define PORT 2000
#define IP ""

// Colors
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KBLU  "\x1B[34m"
#define KCYN  "\x1B[36m"

void* processSocketThreadRequests(int acceptedSocketFD, int connectionSemaphore);
void createBuffers();
void releaseBuffers();
void writeBuffer(char* operation, char operator, char* result);
void removeAllCRLF(char* str);

StringBuffer* sumBuffer;
StringBuffer* subBuffer;
StringBuffer* timesBuffer;
StringBuffer* divBuffer;

int main() {
    printf("%s[+] starting with IP: '%s' and PORT: %d\n", KCYN, IP, PORT);

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
    printf("%s[+] socket was bound successfully\n", KCYN);

    int listenResult = listen(serverSocketFD, LISTEN_BACKLOG);
    if (listenResult == -1) {
        perror("[!] error while listening incoming connections\n");

        return 0;
    }

    createBuffers();

    pthread_mutex_t* establishedConnectionsMutex = newMutex();
    pthread_cond_t* noConnectionsCond = newCond();

    StartAcceptingConnectionsParams params;
    params.availableConnectionsSemaphore = createSemaphore(MAX_CONNECTIONS);
    params.serverSocketFD = serverSocketFD;
    params.callback = processSocketThreadRequests;

    EstablishedConnections establishedConnections;
    params.establishedConnections = &establishedConnections;
    params.establishedConnections->firstClientConnected = false;
    params.establishedConnections->count = 0;
    params.establishedConnections->mutex = establishedConnectionsMutex;
    params.establishedConnections->noConnectionsCond = noConnectionsCond;


    pthread_t threadAcceptConnectionsID;
    pthread_create(&threadAcceptConnectionsID, NULL, startAcceptingIncomingConnections, &params);

    pthread_mutex_lock(params.establishedConnections->mutex);
    while (!params.establishedConnections->firstClientConnected || (params.establishedConnections->firstClientConnected && params.establishedConnections->count > 0)) {
        pthread_cond_wait(params.establishedConnections->noConnectionsCond, params.establishedConnections->mutex);
    }

    pthread_mutex_unlock(params.establishedConnections->mutex);

    printf("%s[!] shutting down....",KRED);

    releaseBuffers();
    shutdown(serverSocketFD, SHUT_RDWR);

    return 0;
}

void* processSocketThreadRequests(int acceptedSocketFD, int connectionSemaphore) {
    char request[1024];
    char response[1024];
    regex_t regex;

    int regexCompilationResult = compileRegexOperations(&regex);
    if (regexCompilationResult != 0 ) {
        perror("[!] error while compiling regex\n");

        close(acceptedSocketFD);
        VSemaphore(connectionSemaphore);
        regfree(&regex);

        return NULL;
    }

    while (true) {
        ssize_t amountReceived = recv(acceptedSocketFD, request, sizeof(request), 0);
        if (amountReceived < 0) {
            char errorMessage[1024];

            sprintf(errorMessage, "[-] fd: %d - error while receiving data\n", acceptedSocketFD);
            perror(errorMessage);

            break;
        }

        if (amountReceived == 0) {
            printf("%s[-] fd: %d - socket closed\n", KRED, acceptedSocketFD);
            break;
        }

        request[amountReceived] = '\0';

        int regexResult;
        char operator;

        double result = getOperationResultFromRequest(&regex, request, &regexResult, &operator);
        switch (regexResult) {
            case 0: {
                if (regexResult != 0) {
                    strcpy(response, "error al procesar la operación, verifique su expresión");
                    break;
                }

                char resultStr[100];
                sprintf(resultStr, "%.2f", result);

                removeAllCRLF(request);
                printf("%s[*] resolving expression %s with result: %s\n", KCYN, request, resultStr);

                writeBuffer(request, operator, resultStr);

                sprintf(response, "RESULT = %s", resultStr);

                break;
            }
            case REG_NOMATCH:
                strcpy(response, "expresión inválida, verifique la sintaxis de su expresión");
                break;
            default:
                strcpy(response, "error al validar su expresion, intente nuevamente");
        }

        send(acceptedSocketFD, response, sizeof (response), 0);
    }

    regfree(&regex);

    return NULL;
}

void createBuffers() {
    sumBuffer = newBufferWithMutex(newMutex());
    subBuffer = newBufferWithMutex(newMutex());
    timesBuffer = newBufferWithMutex(newMutex());
    divBuffer = newBufferWithMutex(newMutex());
}

void releaseBuffers() { // TODO: Write file?
    freeBuffer(sumBuffer);
    freeBuffer(subBuffer);
    freeBuffer(timesBuffer);
    freeBuffer(divBuffer);
}

void writeBuffer(char* operation, char operator, char* result) {
    StringBuffer* buffer;
    switch (operator) {
        case '+':
            buffer = sumBuffer;
            break;
        case '-':
            buffer = subBuffer;
            break;
        case '*':
            buffer = timesBuffer;
            break;
        case '/':
            buffer = divBuffer;
            break;
        default:
            return;
    }

    char* log = malloc(sizeof(char)*1024);
    sprintf(log, "%s = %s \n", operation, result);

    appendToBuffer(buffer, log);
}

void removeAllCRLF(char* str) {
    int i = 0, j = 0;
    while (str[i]) {
        if (str[i] != '\r' && str[i] != '\n') {
            str[j++] = str[i];
        }
        i++;
    }
    str[j] = '\0';
}